// ReCON_Tiers reads the mission's loot tiers from areaflags.map for the Live
// Map, a little each frame so the server never stalls (the file is up to
// 84 MB). ReCON's livemap/tiers.go has the format: a 24-byte header (width,
// height in cells, world width and height in meters, usage bits, value bits,
// often 0), then every cell's usage flags, then every cell's value (tier)
// flags, row 0 the south edge, 4 (low nibble first), 8 or 16 bits a cell.
// The grid sent is CELLS × CELLS, each cell the file cell at its center,
// run-length coded: value, count, value, count...
class ReCON_Tiers
{
	static const int CELLS = 512;
	static const int BUF_BYTES = 16384;
	static const int CHUNKS_A_FRAME = 256; // 4 MB a frame at most

	protected FileHandle m_File;
	protected int m_Phase;      // 0 start, 1 measure, 2 skip usage, 3 rows, 4 done
	protected int m_W;
	protected int m_H;
	protected int m_Usage;      // usage bits a cell
	protected int m_Bits;       // value bits a cell
	protected float m_SizeX;
	protected float m_SizeZ;
	protected int m_Body;       // bytes after the header
	protected int m_Left;       // bytes left to skip (phase 2)
	protected int m_Row;        // the next source row (phase 3)
	protected int m_GridRow;    // the next grid row
	protected int m_RunValue = -1;
	protected int m_RunCount;
	protected int m_Buf[4096];  // BUF_BYTES
	protected ref ReCON_TierGrid m_Grid = new ReCON_TierGrid();

	bool IsDone()
	{
		return m_Phase == 4;
	}

	ReCON_TierGrid Result()
	{
		return m_Grid;
	}

	// Step reads some more; call it each frame until IsDone.
	void Step()
	{
		if (m_Phase == 0)
		{
			if (!OpenAndHeader())
				return;
			m_Body = 0;
			m_Phase = 1;
			return;
		}
		for (int c = 0; c < CHUNKS_A_FRAME && m_Phase != 4; c++)
		{
			if (m_Phase == 1)
				Measure();
			else if (m_Phase == 2)
				Skip();
			else if (m_Phase == 3)
				ReadRow();
		}
	}

	protected bool OpenAndHeader()
	{
		m_File = OpenFile("$mission:areaflags.map", FileMode.READ);
		if (!m_File)
			return Fail("the mission has no areaflags.map");
		int h[6];
		if (ReadFile(m_File, h, 24) != 24)
			return Fail("areaflags.map is too short");
		m_W = h[0];
		m_H = h[1];
		m_SizeX = h[2];
		m_SizeZ = h[3];
		m_Usage = h[4];
		if (m_W <= 0 || m_H <= 0 || m_W > 16384 || m_H > 16384 || m_Usage < 0 || m_Usage > 64 || m_SizeX <= 0 || m_SizeZ <= 0)
			return Fail("areaflags.map header looks wrong");
		return true;
	}

	// Measure reads to the end once: the file's length gives the value bits.
	protected void Measure()
	{
		int n = ReadFile(m_File, m_Buf, BUF_BYTES);
		if (n > 0)
		{
			m_Body += n;
			return;
		}
		CloseFile(m_File);
		m_File = null;
		// cells * bits / 8 == body, so bits = body * 8 / cells, less usage.
		// Kept in floats: 84 MB * 8 overflows an int.
		float cells = m_W;
		cells *= m_H;
		float perCell = m_Body * 8.0 / cells;
		m_Bits = Math.Round(perCell) - m_Usage;
		if (Math.AbsFloat(perCell - Math.Round(perCell)) > 0.001 || (m_Bits != 4 && m_Bits != 8 && m_Bits != 16))
		{
			Fail("areaflags.map has " + perCell + " bits a cell; ReCON-Mod reads 4, 8 or 16 tier bits");
			return;
		}
		if (m_W * m_Bits / 8 > BUF_BYTES)
		{
			Fail("areaflags.map rows are too wide");
			return;
		}
		if (!OpenAndHeader())
			return;
		m_Left = m_W * m_H / 8 * m_Usage;
		m_Phase = 2;
	}

	protected void Skip()
	{
		if (m_Left <= 0)
		{
			m_Phase = 3;
			return;
		}
		int want = BUF_BYTES;
		if (m_Left < want)
			want = m_Left;
		int n = ReadFile(m_File, m_Buf, want);
		if (n <= 0)
		{
			Fail("areaflags.map ended early");
			return;
		}
		m_Left -= n;
	}

	protected void ReadRow()
	{
		if (m_GridRow >= CELLS)
		{
			Finish();
			return;
		}
		int rowBytes = m_W * m_Bits / 8;
		if (ReadFile(m_File, m_Buf, rowBytes) != rowBytes)
		{
			Fail("areaflags.map ended early");
			return;
		}
		int sampleRow = (2 * m_GridRow + 1) * m_H / (2 * CELLS);
		if (m_Row == sampleRow)
		{
			for (int gx = 0; gx < CELLS; gx++)
				AddCell(CellValue((2 * gx + 1) * m_W / (2 * CELLS)));
			m_GridRow++;
		}
		m_Row++;
	}

	// CellValue is cell x's tier bits in the row in m_Buf.
	protected int CellValue(int x)
	{
		if (m_Bits == 4)
		{
			int b = ByteAt(x / 2);
			if (x % 2 == 1)
				return (b >> 4) & 15;
			return b & 15;
		}
		if (m_Bits == 8)
			return ByteAt(x);
		return ByteAt(2 * x) | (ByteAt(2 * x + 1) << 8);
	}

	protected int ByteAt(int k)
	{
		return (m_Buf[k / 4] >> ((k % 4) * 8)) & 255;
	}

	protected void AddCell(int v)
	{
		if (v == m_RunValue)
		{
			m_RunCount++;
			return;
		}
		if (m_RunCount > 0)
		{
			m_Grid.runs.Insert(m_RunValue);
			m_Grid.runs.Insert(m_RunCount);
		}
		m_RunValue = v;
		m_RunCount = 1;
	}

	protected void Finish()
	{
		if (m_RunCount > 0)
		{
			m_Grid.runs.Insert(m_RunValue);
			m_Grid.runs.Insert(m_RunCount);
		}
		m_Grid.sizeX = m_SizeX;
		m_Grid.sizeZ = m_SizeZ;
		m_Grid.n = CELLS;
		Close();
		m_Phase = 4;
		ReCON_Log.Info("read the loot tiers from areaflags.map (" + m_Grid.runs.Count() / 2 + " runs)");
	}

	protected bool Fail(string why)
	{
		Close();
		m_Grid.runs.Clear();
		m_Grid.error = why;
		m_Phase = 4;
		ReCON_Log.Info("loot tiers: " + why + "; ReCON uses its own copy");
		return false;
	}

	protected void Close()
	{
		if (m_File)
			CloseFile(m_File);
		m_File = null;
	}
};

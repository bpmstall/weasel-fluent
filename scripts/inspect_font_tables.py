import struct

font_path = "C:/Windows/Fonts/seguiemj.ttf"
with open(font_path, "rb") as f:
    header = f.read(12)
    sfnt_version, num_tables = struct.unpack(">IH", header[:6])
    print(f"num_tables: {num_tables}")
    tables = {}
    for i in range(num_tables):
        tag, check_sum, offset, length = struct.unpack(">4sIII", f.read(16))
        tables[tag.decode('latin1', errors='ignore')] = (offset, length)

    print("Tables in seguiemj.ttf:", list(tables.keys()))
    if 'COLR' in tables:
        f.seek(tables['COLR'][0])
        colr_ver = struct.unpack(">H", f.read(2))[0]
        print(f"COLR table version: {colr_ver}")

#!/usr/bin/env python3
"""Собирает гибридный загрузочный ISO (ISO9660 + El Torito: BIOS x86_64 + UEFI x86_64). Без grub/xorriso."""
import struct, sys, os, datetime

SEC = 2048

def make_efiboot_img(efi_bytes):
    sec_size = 512
    total_secs = 2880  # 1.44 MB standard floppy FAT12 image
    img = bytearray(total_secs * sec_size)

    # Boot sector BPB
    img[0:3] = b'\xeb\x3c\x90'
    img[3:11] = b'MSWIN4.1'
    struct.pack_into('<HBHBHHBHHHII', img, 11,
        sec_size, 1, 1, 2, 224, total_secs, 0xF0, 9, 18, 2, 0, 0)
    img[36:38] = b'\x00\x00'
    img[38] = 0x29
    img[39:43] = b'\x12\x34\x56\x78'
    img[43:54] = b'CRAVGOS EFI'
    img[54:62] = b'FAT12   '
    img[510:512] = b'\x55\xaa'

    fat1_offset = 1 * sec_size
    fat2_offset = 10 * sec_size
    root_offset = 19 * sec_size
    data_offset = 33 * sec_size

    def set_fat12(fat, cluster, val):
        idx = (cluster * 3) // 2
        if cluster % 2 == 0:
            fat[idx] = val & 0xFF
            fat[idx+1] = (fat[idx+1] & 0xF0) | ((val >> 8) & 0x0F)
        else:
            fat[idx] = (fat[idx] & 0x0F) | ((val << 4) & 0xF0)
            fat[idx+1] = (val >> 4) & 0xFF

    fat = bytearray(9 * sec_size)
    fat[0] = 0xF0; fat[1] = 0xFF; fat[2] = 0xFF

    def make_entry(name, ext, attr, cluster, size):
        e = bytearray(32)
        e[0:8] = name.ljust(8).encode('ascii')
        e[8:11] = ext.ljust(3).encode('ascii')
        e[11] = attr
        struct.pack_into('<H', e, 26, cluster)
        struct.pack_into('<I', e, 28, size)
        return e

    # Root dir has 'EFI' dir (cluster 2)
    img[root_offset:root_offset+32] = make_entry('EFI', '', 0x10, 2, 0)

    # Cluster 2: EFI dir (has BOOT dir at cluster 3)
    c2_off = data_offset + (2 - 2) * sec_size
    img[c2_off:c2_off+32] = make_entry('.', '', 0x10, 2, 0)
    img[c2_off+32:c2_off+64] = make_entry('..', '', 0x10, 0, 0)
    img[c2_off+64:c2_off+96] = make_entry('BOOT', '', 0x10, 3, 0)

    # Cluster 3: BOOT dir (has BOOTX64.EFI at cluster 4)
    c3_off = data_offset + (3 - 2) * sec_size
    img[c3_off:c3_off+32] = make_entry('.', '', 0x10, 3, 0)
    img[c3_off+32:c3_off+64] = make_entry('..', '', 0x10, 2, 0)
    img[c3_off+64:c3_off+96] = make_entry('BOOTX64', 'EFI', 0x20, 4, len(efi_bytes))

    # Cluster 4+: BOOTX64.EFI data
    c_cur = 4
    set_fat12(fat, 2, 0xFFF)
    set_fat12(fat, 3, 0xFFF)
    efi_pos = 0
    while efi_pos < len(efi_bytes):
        chunk = efi_bytes[efi_pos:efi_pos+sec_size]
        c_off = data_offset + (c_cur - 2) * sec_size
        img[c_off:c_off+len(chunk)] = chunk
        efi_pos += sec_size
        if efi_pos < len(efi_bytes):
            set_fat12(fat, c_cur, c_cur + 1)
            c_cur += 1
        else:
            set_fat12(fat, c_cur, 0xFFF)

    img[fat1_offset:fat1_offset+len(fat)] = fat
    img[fat2_offset:fat2_offset+len(fat)] = fat
    return img

def main():
    if len(sys.argv) < 3:
        print("usage: mkiso.py cravgos.img cravgos.iso [bootx64.efi]")
        sys.exit(1)

    img = open(sys.argv[1], 'rb').read()
    out = sys.argv[2]
    efi_path = sys.argv[3] if len(sys.argv) > 3 else ('bootx64.efi' if os.path.exists('bootx64.efi') else None)

    efiboot = None
    if efi_path and os.path.exists(efi_path):
        efi_bytes = open(efi_path, 'rb').read()
        efiboot = make_efiboot_img(efi_bytes)

    now = datetime.datetime.now(datetime.timezone.utc)

    def b32(n): return struct.pack('<I', n) + struct.pack('>I', n)
    def b16(n): return struct.pack('<H', n) + struct.pack('>H', n)
    def pad(s, n): return s.encode('latin1').ljust(n, b' ')
    def dt7(): return bytes([now.year - 1900, now.month, now.day, now.hour, now.minute, now.second, 0])
    def dt17(): return now.strftime('%Y%m%d%H%M%S').encode('ascii') + b'00\x00'
    def dirrec(extent, length, flags, name):
        L = 33 + len(name)
        L += L % 2
        r = bytes([L, 0]) + b32(extent) + b32(length) + dt7() + bytes([flags, 0, 0]) + b16(1) + bytes([len(name)]) + name
        return r.ljust(L, b'\0')

    IMG_SEC = 23
    img_sectors = -(-len(img) // SEC)

    efi_sectors = 0
    EFI_SEC = IMG_SEC + img_sectors
    if efiboot:
        efi_sectors = -(-len(efiboot) // SEC)

    total = IMG_SEC + img_sectors + efi_sectors
    root = dirrec(21, SEC, 2, b'\x00')

    pvd = bytearray(SEC)
    pvd[0] = 1; pvd[1:6] = b'CD001'; pvd[6] = 1
    pvd[8:40] = pad('', 32); pvd[40:72] = pad('CRAVGOS_64', 32)
    pvd[80:88] = b32(total)
    pvd[120:124] = b16(1); pvd[124:128] = b16(1); pvd[128:132] = b16(SEC)
    pvd[132:140] = b32(10)
    pvd[140:144] = struct.pack('<I', 19)
    pvd[148:152] = struct.pack('>I', 20)
    pvd[156:190] = root
    for off, n in ((190, 128), (318, 128), (446, 128), (574, 128), (702, 37), (739, 37), (776, 37)):
        pvd[off:off + n] = pad('', n)
    pvd[813:830] = dt17(); pvd[830:847] = dt17()
    pvd[847:864] = b'0' * 16 + b'\x00'; pvd[864:881] = dt17()
    pvd[881] = 1

    brvd = bytearray(SEC)
    brvd[0] = 0; brvd[1:6] = b'CD001'; brvd[6] = 1
    brvd[7:39] = b'EL TORITO SPECIFICATION'.ljust(32, b'\0')
    brvd[71:75] = struct.pack('<I', 22)

    term = bytearray(SEC); term[0] = 255; term[1:6] = b'CD001'; term[6] = 1

    ptl = bytearray(SEC); ptl[0:10] = struct.pack('<BBIH', 1, 0, 21, 1) + b'\x00\x00'
    ptm = bytearray(SEC); ptm[0:10] = struct.pack('>BBIH', 1, 0, 21, 1) + b'\x00\x00'

    rootdir = bytearray(SEC)
    d = dirrec(21, SEC, 2, b'\x00') + dirrec(21, SEC, 2, b'\x01') + dirrec(IMG_SEC, len(img), 0, b'CRAVGOS.BIN;1')
    if efiboot:
        d += dirrec(EFI_SEC, len(efiboot), 0, b'EFIBOOT.IMG;1')
    rootdir[:len(d)] = d

    cat = bytearray(SEC)
    # 1. Validation Entry (BIOS x86)
    v = bytearray(32); v[0] = 1; v[1] = 0; v[4:28] = b'CRAVGOS'.ljust(24, b'\0'); v[30] = 0x55; v[31] = 0xAA
    s = sum(struct.unpack('<16H', bytes(v))) & 0xFFFF
    v[28:30] = struct.pack('<H', (-s) & 0xFFFF)

    # 2. Initial / Default Entry (BIOS boot image)
    e = bytearray(32); e[0] = 0x88; e[1] = 0
    e[6:8] = struct.pack('<H', img_sectors * 4); e[8:12] = struct.pack('<I', IMG_SEC)

    cat[0:32] = v; cat[32:64] = e

    # 3. Section Header + Entry for UEFI (Platform ID 0xEF)
    if efiboot:
        sh = bytearray(32)
        sh[0] = 0x91  # Final Section Header
        sh[1] = 0xEF  # EFI Platform ID
        sh[2:4] = struct.pack('<H', 1)  # 1 section entry
        sh[4:28] = b'EFI'.ljust(24, b'\0')

        se = bytearray(32)
        se[0] = 0x88  # Bootable
        se[1] = 0     # No emulation
        se[6:8] = struct.pack('<H', len(efiboot) // 512)
        se[8:12] = struct.pack('<I', EFI_SEC)

        cat[64:96] = sh
        cat[96:128] = se

    iso = bytearray(16 * SEC)
    for sec in (pvd, brvd, term, ptl, ptm, rootdir, cat):
        iso += sec
    iso += img.ljust(img_sectors * SEC, b'\0')
    if efiboot:
        iso += efiboot.ljust(efi_sectors * SEC, b'\0')

    assert len(iso) == total * SEC
    open(out, 'wb').write(iso)
    print(f"{out}: {len(iso)} bytes, BIOS boot image {len(img)} bytes ({img_sectors} sectors)" +
          (f", UEFI EFI image {len(efiboot)} bytes ({efi_sectors} sectors)" if efiboot else ""))

if __name__ == '__main__':
    main()

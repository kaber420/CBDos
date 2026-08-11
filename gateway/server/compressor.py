import lz4.frame

def compress_data(data: bytes) -> bytes:
    """Compresses data using LZ4."""
    return lz4.frame.compress(data)

def decompress_data(data: bytes) -> bytes:
    """Decompresses data using LZ4."""
    return lz4.frame.decompress(data)

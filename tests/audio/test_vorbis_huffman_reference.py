"""Host-only symbol oracle using libvorbis 1.3.7 internal test ABI."""
import ctypes as c
import ctypes.util
import pathlib
import sys
from test_vorbis_reference import configure
from test_vorbis_codebook_reference import StaticBook, OggBits, Bits


class CodecBook(c.Structure):
    _fields_ = [(name,c.c_long) for name in ("dim","entries","used")] + [
        (name,c.c_void_p) for name in ("static","values","codes","index","lengths","first")] + [
        (name,c.c_int) for name in ("firstlen","maxlen","quantvals","minimum","delta")]


class Node(c.Structure):
    _fields_ = [("child",c.c_uint32*2),("symbol",c.c_uint32)]


class Tree(c.Structure):
    _fields_ = [("nodes",c.POINTER(Node)),("count",c.c_uint32),("entries",c.c_uint32)]


def main():
    ref=c.CDLL(ctypes.util.find_library("vorbis") or "libvorbis.so.0")
    ogg=c.CDLL(ctypes.util.find_library("ogg") or "libogg.so.0")
    ours=c.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
    assert configure(ref,"vorbis_version_string",c.c_char_p)()==b"Xiph.Org libVorbis 1.3.7"
    init=configure(ref,"vorbis_book_init_encode",c.c_int,c.POINTER(CodecBook),c.POINTER(StaticBook))
    clear=configure(ref,"vorbis_book_clear",None,c.POINTER(CodecBook))
    encode=configure(ref,"vorbis_book_encode",c.c_int,c.POINTER(CodecBook),c.c_int,c.POINTER(OggBits))
    wi=configure(ogg,"oggpack_writeinit",None,c.POINTER(OggBits))
    wc=configure(ogg,"oggpack_writeclear",None,c.POINTER(OggBits))
    byte_count=configure(ogg,"oggpack_bytes",c.c_long,c.POINTER(OggBits))
    bit_count=configure(ogg,"oggpack_bits",c.c_long,c.POINTER(OggBits))
    build=configure(ours,"capy_vorbis_huffman_build",c.c_int,c.c_void_p,c.c_uint32,
                    c.c_void_p,c.c_size_t,c.POINTER(Tree))
    bi=configure(ours,"capy_vorbis_bits_init",c.c_int,c.POINTER(Bits),c.c_void_p,c.c_size_t,c.c_size_t)
    decode=configure(ours,"capy_vorbis_huffman_decode",c.c_int,c.POINTER(Tree),c.POINTER(Bits),c.POINTER(c.c_uint32))
    fixtures=[[2,4,4,4,4,2,3,3],[1],[1,2,3,4,4],list(range(1,32))+[32,32]]
    # Equal-length books plus permutations of an irregular complete tree.
    fixtures += [[depth]*(1<<depth) for depth in range(1,9)]
    base=[1,2,3,4,4]
    for offset in range(len(base)):
        fixtures.append(base[offset:]+base[:offset])
    total=0
    for fixture in fixtures:
        lengths=(c.c_ubyte*len(fixture))(*fixture)
        original=StaticBook(1,len(fixture),lengths,0,0,0,0,0,None,0)
        book,ob=CodecBook(),OggBits()
        wi(c.byref(ob))
        try:
            assert init(c.byref(book),c.byref(original))==0
            assert book.codes, ("libvorbis encoder produced no codewords", fixture)
            sequence=[i for i,length in enumerate(fixture) if length]*3
            for symbol in sequence:
                assert encode(c.byref(book),symbol,c.byref(ob))==fixture[symbol]
            nodes=(Node*(2*len(fixture)))()
            tree,bits=Tree(),Bits()
            assert build(lengths,len(fixture),nodes,len(nodes),c.byref(tree))==0
            assert bi(c.byref(bits),ob.buffer,byte_count(c.byref(ob)),65536)==0
            bits.count=bit_count(c.byref(ob))
            for symbol in sequence:
                observed=c.c_uint32()
                before=bits.position
                assert decode(c.byref(tree),c.byref(bits),c.byref(observed))==0
                assert observed.value==symbol and bits.position-before==fixture[symbol]
                total+=1
            assert bits.position==bit_count(c.byref(ob))
        finally:
            clear(c.byref(book)); wc(c.byref(ob))
    # The encoder's _make_words path does not support a sparse single-entry
    # exception. Compare its required two bit values through the decoder API.
    di=configure(ref,"vorbis_book_init_decode",c.c_int,c.POINTER(CodecBook),c.POINTER(StaticBook))
    rd=configure(ref,"vorbis_book_decode",c.c_long,c.POINTER(CodecBook),c.POINTER(OggBits))
    ri=configure(ogg,"oggpack_readinit",None,c.POINTER(OggBits),c.c_void_p,c.c_int)
    lengths=(c.c_ubyte*3)(0,1,0)
    original=StaticBook(1,3,lengths,0,0,0,0,0,None,0)
    book=CodecBook()
    try:
        assert di(c.byref(book),c.byref(original))==0
        nodes=(Node*1)(); tree=Tree()
        assert build(lengths,3,nodes,1,c.byref(tree))==0
        for value in (0,1):
            data=(c.c_ubyte*1)(value); ob=OggBits(); bits=Bits(); observed=c.c_uint32()
            ri(c.byref(ob),data,1)
            assert bi(c.byref(bits),data,1,1)==0
            assert decode(c.byref(tree),c.byref(bits),c.byref(observed))==0
            assert observed.value==rd(c.byref(book),c.byref(ob))==1
            assert bits.position==bit_count(c.byref(ob))==1
    finally:
        clear(c.byref(book))
    print(f"[vorbis-huffman-reference] {total} encoded symbols from {len(fixtures)} Xiph books plus two sparse decoder cases matched exactly")


if __name__=="__main__":
    main()

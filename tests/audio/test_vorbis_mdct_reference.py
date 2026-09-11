"""Independent direct-form IMDCT oracle for the fast freestanding transform."""
import ctypes as c
import math
import pathlib
import random
import sys


class Plan(c.Structure):
    _fields_ = [("n", c.c_uint), ("log2n", c.c_uint),
                ("trig", c.POINTER(c.c_float)), ("bitrev", c.POINTER(c.c_int32)),
                ("trig_count", c.c_size_t), ("bitrev_count", c.c_size_t)]


def main():
    ours=c.CDLL(str(pathlib.Path(sys.argv[1]).resolve()))
    init=ours.capy_vorbis_mdct_plan_init
    init.argtypes=[c.c_uint,c.c_void_p,c.c_size_t,c.c_void_p,c.c_size_t,c.POINTER(Plan)]
    init.restype=c.c_int
    backward=ours.capy_vorbis_mdct_backward
    backward.argtypes=[c.POINTER(Plan),c.c_void_p,c.c_size_t,c.c_float,
                       c.c_void_p,c.c_size_t,c.c_void_p,c.c_size_t]
    backward.restype=c.c_int
    rng=random.Random(0x1D0C7)
    cases=samples=0
    maximum=0.0
    for n in (64,128,256,512,1024,2048,4096,8192):
        trig=(c.c_float*(n+n//4))(); bitrev=(c.c_int32*(n//4))(); plan=Plan()
        assert init(n,trig,len(trig),bitrev,len(bitrev),c.byref(plan))==0
        spectra=[]
        impulse=[0.0]*(n//2); impulse[rng.randrange(n//2)]=1.0; spectra.append(impulse)
        for _ in range(8 if n<=256 else 2):
            values=[0.0]*(n//2)
            for _ in range(min(8,n//2)):
                values[rng.randrange(n//2)]=rng.randrange(-8,9)/8
            spectra.append(values)
        for values in spectra:
            source=(c.c_float*(n//2))(*values); output=(c.c_float*n)(); scratch=(c.c_float*n)()
            assert backward(c.byref(plan),source,n//2,1e6,output,n,scratch,n)==0
            nonzero=[(k,value) for k,value in enumerate(values) if value]
            for sample,actual in enumerate(output):
                expected=sum(value*math.cos(2*math.pi/n*(sample+.5+n/4)*(k+.5))
                             for k,value in nonzero)
                error=abs(actual-expected)
                scale=max(1.0,sum(abs(value) for _,value in nonzero))
                assert error<=2.5e-6*scale,(n,sample,actual,expected,error)
                maximum=max(maximum,error);samples+=1
            cases+=1
    print(f"[vorbis-mdct-reference] {cases} transforms / {samples} samples; max error {maximum:.3g}")


if __name__=="__main__":
    main()

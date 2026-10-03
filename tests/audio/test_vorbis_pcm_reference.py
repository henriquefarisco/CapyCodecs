#!/usr/bin/env python3
import array, math, pathlib, struct, subprocess, sys, tempfile, wave

decoder=pathlib.Path(sys.argv[1]).resolve()

def compare_fixture(td, name, frames, transients):
    root=pathlib.Path(td); wav=root/'source.wav'; ogg=root/'source.ogg'
    ours=root/'ours.f32'; reference=root/'reference.f32'
    rate=48000
    with wave.open(str(wav),'wb') as f:
        f.setnchannels(2); f.setsampwidth(2); f.setframerate(rate)
        data=bytearray()
        for i in range(frames):
            left=round(12000*math.sin(2*math.pi*440*i/rate))
            right=round(9000*math.sin(2*math.pi*997*i/rate))
            if transients:
                # Sharp attacks force long/short mode switches, absent from
                # the steady-tone fixture that originally missed this bug.
                left=int(5000*math.sin(2*math.pi*440*i/rate))
                if i in (9600, 24000, 38400):
                    left=30000
                right=-left
            data += struct.pack('<hh',left,right)
        f.writeframes(data)
    subprocess.run(['oggenc','-Q','-q','4','-o',str(ogg),str(wav)],check=True)
    subprocess.run([str(decoder),str(ogg),str(ours)],check=True)
    subprocess.run(['ffmpeg','-v','error','-i',str(ogg),'-f','f32le','-acodec','pcm_f32le',str(reference)],check=True)
    def load(path):
        a=array.array('f'); a.frombytes(path.read_bytes())
        if sys.byteorder!='little': a.byteswap()
        return a
    a,b=load(ours),load(reference)
    assert len(a) == frames*2, (len(a), frames*2)
    # Compare away from codec preroll/tail. Search a modest frame offset using
    # mean absolute error, then require close normalized PCM in the common body.
    best=None
    for offset in range(-4096,4097):
        aa=max(0,offset)*2; bb=max(0,-offset)*2
        count=min(len(a)-aa,len(b)-bb,8000)*1
        count -= count%2
        if count<2000: continue
        error=sum(abs(a[aa+i]-b[bb+i]) for i in range(count))/count
        if best is None or error<best[0]: best=(error,offset,aa,bb,count)
    _,offset,aa,bb,_=best
    count=min(len(a)-aa,len(b)-bb)
    count -= count%2
    error=sum(abs(a[aa+i]-b[bb+i]) for i in range(count))/count
    peak=max(abs(a[aa+i]-b[bb+i]) for i in range(count))
    assert count >= (frames-256)*2 and error < 2.5e-4 and peak < 3e-3, (error,peak,offset,len(a),len(b))
    print(f'[vorbis-pcm-reference] {name} Ogg matched: mean={error:.3g} peak={peak:.3g} offset={offset} frames={count//2}')

for name, frames, transients in (('tones', 12000, False),
                                  ('transients', 48000, True)):
    with tempfile.TemporaryDirectory() as td:
        compare_fixture(td, name, frames, transients)

import re, sys
src = open(sys.argv[1], encoding='utf-8').read()
names = sys.argv[2:] or ['BREATHE', 'DRIFT_L', 'DRIFT_R', 'THINK', 'CURL']
arr = re.compile(r'(\w+)\s*\[\s*5\s*\]\s*=\s*\{(.*?)\}\s*;', re.DOTALL)
strr = re.compile(r'"((?:[^"\\]|\\.)*)"')
blocks = {m.group(1): [s.encode().decode('unicode_escape') for s in strr.findall(m.group(2))]
          for m in arr.finditer(src)}
for name in names:
    rows = blocks.get(name)
    if not rows:
        continue
    print('===', name, '===')
    for r in rows:
        lead = len(r) - len(r.lstrip(' '))
        trail = len(r) - len(r.rstrip(' '))
        # where the content's CENTRE sits within the string (in chars)
        if r.strip():
            centre = (lead + (len(r) - trail)) / 2
        else:
            centre = len(r) / 2
        print(f'  len={len(r):2} lead={lead} trail={trail} content_centre={centre:4.1f}  |{r}|')

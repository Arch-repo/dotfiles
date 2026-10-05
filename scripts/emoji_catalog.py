"""Convert pinned Unicode test data to the native picker's tabular catalog."""
import re

def convert(content):
    group=subgroup=''
    rows=[]
    for line in content.splitlines():
        if line.startswith('# group: '):group=line[9:]
        elif line.startswith('# subgroup: '):subgroup=line[12:]
        elif line and not line.startswith('#'):
            match=re.match(r'([0-9A-F ]+);\s*(fully-qualified|component)\s*#\s*\S+\s+E[0-9.]+\s+(.+)',line)
            if not match:continue
            symbol=''.join(chr(int(value,16)) for value in match[1].split())
            name=match[3].strip()
            rows.append('\t'.join((symbol,group,subgroup,name,name)))
    if len(rows)<3000:raise ValueError('Incomplete Unicode emoji dataset')
    return '\n'.join(rows)+'\n'

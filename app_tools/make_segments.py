#!/bin/env python3

import os, sys, subprocess
from pathlib import Path

_, inpath, outdir = sys.argv

appvar_size = 65232

with open(inpath, 'rb') as infile:
	app_data = infile.read()

app_data += len(app_data).to_bytes(3, 'little')

segments = [app_data[x:x+appvar_size] for x in range(0, len(app_data), appvar_size)]
for (i, segment) in enumerate(segments):
	var_name = f'AppIns{i:02}'
	filename_bin = outdir + '/' + var_name + '.bin'
	filename_8xv = outdir + '/' + var_name + '.8xv'
	with open(filename_bin, 'wb') as outfile:
		outfile.write(segment)
	subprocess.run([os.environ.get('CONVBIN', 'convbin'), '-i', filename_bin,
	                '-o', filename_8xv, '-j', 'bin', '-k', '8xv', '-n', var_name,
	                '-r'], check=True)

# A smaller variant must not leave tail segments from a previous larger build.
for path in Path(outdir).glob('AppIns[0-9][0-9].*'):
	if path.suffix in ('.bin', '.8xv') and int(path.stem[6:]) >= len(segments):
		path.unlink()

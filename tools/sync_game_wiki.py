# SPDX-License-Identifier: MIT
"""Copy reviewed, source-controlled pages to an initialized Wiki checkout."""
import argparse,shutil
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('wiki_checkout',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parents[1]
if not (a.wiki_checkout/'.git').exists():raise SystemExit('Clone or initialize the Wiki checkout first')
for source in sorted((root/'docs/games/wiki').glob('*.md')):
 shutil.copyfile(source,a.wiki_checkout/source.name)
# Screenshots stay versioned with the executable source and are embedded by URL.
print('Wiki pages copied. Review the diff before committing and pushing.')

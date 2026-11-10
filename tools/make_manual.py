#!/usr/bin/env python3
"""The manual as a PDF: docs/HANDLEIDING.md -> docs/HAWK-CHUA-handleiding.pdf (A4, a cover with the splash).

    python3 tools/make_manual.py [version]        # needs: pip install markdown playwright (Chromium)
"""
import base64, pathlib, subprocess, sys
import markdown
from playwright.sync_api import sync_playwright

ROOT = pathlib.Path(__file__).resolve().parent.parent
ver = sys.argv[1] if len(sys.argv) > 1 else subprocess.run(
    ["git", "describe", "--tags", "--abbrev=0"], cwd=ROOT, capture_output=True, text=True).stdout.strip() or "v2"
md = (ROOT / "docs/HANDLEIDING.md").read_text(encoding="utf-8")
md = md.split("\n", 1)[1]                       # (the title: the cover's)
body = markdown.markdown(md, extensions=["tables", "fenced_code", "sane_lists"])
font = base64.b64encode((ROOT / "assets/fonts/InterTight[wght].ttf").read_bytes()).decode()
card = base64.b64encode((ROOT / "docs/card.png").read_bytes()).decode()
html = f"""<!doctype html><html lang="nl"><head><meta charset="utf-8"><title>HAWK CHUA handleiding</title><style>
@font-face {{ font-family: Inter Tight; src: url(data:font/ttf;base64,{font}); font-weight: 100 900; }}
@page {{ size: A4; margin: 16mm 15mm 16mm 15mm; }}
@page cover {{ margin: 0; }}
:root {{ --ink: #1d1a17; --dim: #6b645c; --acc: #e8742a; --rule: #e6dfd6; }}
body {{ font: 9.2pt/1.45 "Inter Tight", sans-serif; color: var(--ink); margin: 0; }}
.cover {{ page: cover; height: 297mm; width: 210mm; box-sizing: border-box; background: #15110e; color: #f3e9de;
         display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 9mm;
         page-break-after: always; }}
.cover .k {{ letter-spacing: .3em; font-size: 9pt; color: #cbbfb1; }}
.cover img {{ width: 120mm; image-rendering: pixelated; border-radius: 4mm; }}
.cover .h {{ letter-spacing: .35em; color: var(--acc); font-weight: 700; }}
.cover .v {{ font-size: 8pt; color: #8c8177; }}
h2 {{ font-size: 15pt; margin: 0 0 3mm; padding-top: 2mm; border-top: 2px solid var(--acc); margin-top: 7mm; page-break-after: avoid; }}
h3 {{ page-break-after: avoid; }}
h3 {{ font-size: 11pt; margin: 5mm 0 2mm; }}
p {{ margin: 0 0 2.4mm; }}
strong {{ font-weight: 700; }}
table {{ border-collapse: collapse; width: 100%; margin: 2mm 0 4mm; font-size: 8.4pt; }}
th {{ text-align: left; font-size: 7pt; letter-spacing: .08em; text-transform: uppercase; color: var(--dim);
     border-bottom: 1.5px solid var(--ink); padding: 1.2mm 2mm 1.2mm 0; }}
td {{ border-bottom: 1px solid var(--rule); padding: 1.4mm 2mm 1.4mm 0; vertical-align: top; }}
td:first-child {{ font-weight: 700; white-space: nowrap; padding-right: 4mm; }}
tr {{ page-break-inside: avoid; }}
code, pre {{ font-family: ui-monospace, Menlo, monospace; font-size: 8pt; }}
pre {{ background: #f6f1ea; padding: 2.5mm 3mm; border-radius: 2mm; white-space: pre-wrap; }}
ul {{ margin: 0 0 3mm; padding-left: 5mm; }} li {{ margin: 0 0 1.2mm; }}
a {{ color: var(--acc); text-decoration: none; }}
</style></head><body>
<div class="cover"><div class="k">FIRMWARE VOOR DE M-VAVE FM-1</div>
<img src="data:image/png;base64,{card}"><div class="h">HANDLEIDING</div>
<div class="v">{ver} · plieuwski · github.com/Plieuw/hawk-chua</div></div>
{body}</body></html>"""
out = ROOT / "docs/HAWK-CHUA-handleiding.pdf"
with sync_playwright() as p:
    b = p.chromium.launch()
    pg = b.new_page()
    pg.set_content(html, wait_until="load")
    pg.pdf(path=str(out), format="A4", print_background=True, display_header_footer=True,
           header_template="<span></span>",
           footer_template='<div style="font:7pt sans-serif;color:#8c8177;width:100%;padding:0 15mm;display:flex;'
                           'justify-content:space-between"><span>HAWK CHUA · handleiding</span>'
                           '<span class="pageNumber"></span></div>',
           prefer_css_page_size=True)
    b.close()
print(out)

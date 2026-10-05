#!/usr/bin/env python3
"""Bounded, markup-safe data for the lock screen; no authentication logic."""
from datetime import datetime
from pathlib import Path
import html
import subprocess
import sys

def battery(root=Path('/sys/class/power_supply')):
    for supply in sorted(root.glob('*')):
        try:
            if (supply/'type').read_text().strip() != 'Battery':
                continue
            capacity=max(0,min(100,int((supply/'capacity').read_text())))
            state=(supply/'status').read_text().strip()
            return f'{"" if state in ("Charging","Full") else "󰁹"} {capacity}%'
        except (OSError,ValueError):
            continue
    return 'Alimentazione di rete'

def date_text():
    now=datetime.now()
    days=('Lunedì','Martedì','Mercoledì','Giovedì','Venerdì','Sabato','Domenica')
    months=('Gennaio','Febbraio','Marzo','Aprile','Maggio','Giugno','Luglio','Agosto','Settembre','Ottobre','Novembre','Dicembre')
    return f'{days[now.weekday()]} {now.day} {months[now.month-1]}'

def media():
    try:
        result=subprocess.run(['playerctl','metadata','--format','{{title}} · {{artist}}'],
                              capture_output=True,text=True,timeout=.7)
        return ('󰎆  '+result.stdout.strip()[:60]) if result.returncode==0 else ''
    except (OSError,subprocess.TimeoutExpired):
        return ''

if __name__=='__main__':
    action=sys.argv[1] if len(sys.argv)>1 else 'date'
    if action=='date': text=date_text()
    elif action=='battery': text=battery()
    elif action=='status': text=battery()+' · '+(sys.argv[2] if len(sys.argv)>2 else 'IT')[:32]
    elif action=='media': text=media()
    else: raise SystemExit('Usage: lock_info.py [date|battery|status|media]')
    print(html.escape(text))

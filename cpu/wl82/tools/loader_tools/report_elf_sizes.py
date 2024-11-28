#!/usr/bin/env python3

# 输入 ELF 文件，输出其中数据大小，并生成一个 HTML 文件，可用浏览器打开

# 例子：
# 1. 直接输出一个 ELF 文件的汇总
#       /opt/utils/report_elf_sizes --elf sdk.elf --output symbol_tbl.html
#       将 sdk.elf 的汇总情况，输出到 symbol_tbl.html 文件中
# 2. 直接输出一个 symbol_tbl.txt 文件汇总
#       /opt/utils/report_elf_sizes --symbol_tbl symbol_tbl.txt --output symbol_tbl.html
#       将 symbol_tbl.txt 的汇总情况，输出到 symbol_tbl.html 文件中
# 然后，可以使用浏览器来打开　html 文件
# 例如： 
#       firefox symbol_tbl.html

import argparse
from fileinput import filename
import subprocess
import platform
import os
import sys
import json
from string import Template
import html
import tempfile

# if name starts with _Z, should use cxxfilt to mangle
# required external tools
# 1. cxxfilt
# 2. llvm-objsizedump

# if name starts with .L_MergedGlobals., whould ignore

TMPL = Template("""
<!DOCTYPE html>
<html>
    <meta charset="UTF-8">

    <title>
        函数、变量大小汇总
    </title>

    <style>
    td {
    font-family: monospace;
    }

    table tr:nth-child(even) {
        background-color: beige;
    }

    table tr:nth-child(odd) {
        background-color: gainsboro;
    }

    table tr:hover {
        background-color: aquamarine;
    }

    table tr:hover td {
        background: none;
    }
    </style>

    <script>

        all_symbols = $all_symbols;

        function get_selection(idname) {
            let a = document.getElementById(idname);
            if (a.selectedIndex == 0) {
                // select all section
                a = null;
            } else {
                a = a.value;
            }
            return a;
        }

        function escapeHTML(str){
            return new Option(str).innerHTML;
        }

        function padStart(o) {
            return ("00000000" + o).slice(-8);
        }

        function query_symbols() {
            console.log('clicked');

            filter_symbols = [];

            sel_section = get_selection('sel-section');
            sel_file = get_selection('sel-file');
            sel_type = get_selection('sel-type');

            all_symbols.forEach((a, _index, all_symbols) => {
                if (sel_section != null && a.sec != sel_section) {
                    return;
                }
                if (sel_file != null && a.file != sel_file) {
                    return;
                }
                if (sel_type != null && a.symtype != sel_type) {
                    return;
                }
                filter_symbols.push(a);
            });

            sel_order = document.getElementById('sel-order').value;
            if (sel_order == 'by-addr') {
                filter_symbols = filter_symbols.sort((lhs, rhs) => {
                    return lhs.addr > rhs.addr;
                });
            } else if (sel_order == 'by-size') {
                filter_symbols = filter_symbols.sort((lhs, rhs) => {
                    return lhs.size < rhs.size;
                });
            }

            // console.log(filter_symbols);

            let syms = document.getElementById('symbol-list');

            let htmls = '<table border="1" width="100%">\\n' +
                        '  <tr>\\n'             +
                        '     <th width="50%">名字</th>\\n' +
                        '     <th>地址</th>\\n' +
                        '     <th>大小</th>\\n' +
                        '     <th>段</th>\\n'   +
                        '     <th width="40%">文件</th<\\n' +
                        '  <tr>\\n';

            for (var i = 0; i < filter_symbols.length; i++) {
                let a = filter_symbols[i];
                htmls += '<tr>\\n' +
                         '  <td>' + escapeHTML(a.name) + '</td>\\n' +
                         '  <td> 0x' + padStart(a.addr.toString(16)) + '</td>\\n' +
                         '  <td> 0x' + a.size.toString(16) + '(' + a.size.toLocaleString("en-US") + ')</td>\\n' +
                         '  <td>' + escapeHTML(a.sec) + '</td>\\n' +
                         '  <td>' + escapeHTML(a.file) + '</td>\\n' +
                         '</tr>\\n';
            }

            htmls += '</table>';

            syms.innerHTML = htmls;
        }
    </script>

    <body>

        <div>
            <table border="1">
                <tr>
                    <th>段</th>
                    <th>文件</th>
                    <th>类型</th>
                    <th>排序方式</th>
                    <th></th>
                </tr>

                <tr>
                    <td>
                        <select id="sel-section" onChange="query_symbols()">
                            <option value="">全部</option>
                            $sections
                        </select>
                    </td>
                    <td>
                        <select id="sel-file" onChange="query_symbols()">
                            <option value="">全部</option>"
                            $files
                        </select>
                    </td>
                    <td>
                        <select id="sel-type" onChange="query_symbols()">
                            <option value="">全部</option>
                            <option value="F">函数</option>
                            <option value="O">全局变量</option>
                        </select>
                    </td>
                    <td>
                        <select id="sel-order" onChange="query_symbols()">
                            <option value="by-size">按大小</option>
                            <option value="by-addr">按地址</option>
                        </select>
                    </td>
                    <td>
                        <button onclick="query_symbols()">
                            显示
                        </button>
                    </td>
                </tr>
            </table>
        </div>

        <div id="symbol-list" width="100%">
        </div>

        <script>
            query_symbols();
        </script>

    </body>
</html>
""")


def get_options_list(vals):
    return list(map(lambda x: '<option value="{a}">{a}</option>'.format(a=html.escape(x)), vals))


def parse_symbol_tbl_txt(fout, filepath, cxxfilt):
    cxxnames = []
    all_inputs = []

    all_files = set()
    shortfiles = set()

    with open(filepath, 'r') as fin:
        for l in fin:
            ls = l.split()
            #   0  1   2    3     4    5   6
            # ADDR l- -O SECTION SIZE FUN DEF_FILE
            if len(ls) < 6:
                continue
            name = ls[5]
            if name.startswith('.L_MergedGlobals'):
                continue
            if name.startswith('_Z'):
                cxxnames.append(name)

            addr = int(ls[0], base=16)
            section = ls[3]
            symscope = ls[1][0]  # l or g
            symtype = ls[2][1]  # O, F
            size = int(ls[4], base=16)
            def_file = ' '.join(ls[6:])
            def_file = os.path.normpath(def_file)
            def_file = def_file.replace('\\', '/')
            if def_file == '<invalid>':
                def_file = '*编译该文件时候未加-g参数，无法找到文件信息*'
            all_inputs.append((name, addr, size, section,
                               symscope, symtype, def_file))

            all_files.add(def_file)

    all_files = list(all_files)
    all_files = sorted(all_files, key=lambda x: x.count('/'))

    filenames = {}
    for def_file in all_files:
        if def_file not in filenames:
            fs = def_file.split('/')
            i = -1
            while True:
                name = '/'.join(fs[i:])
                if name not in shortfiles:
                    break
                i -= 1

            filenames[def_file] = name
            shortfiles.add(name)

    cxxname_maps = {}
    if len(cxxnames) != 0:
        # parse all cxxnames
        try:
            with subprocess.Popen([cxxfilt], stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE) as p:
                outs, _ = p.communicate(input='\n'.join(cxxnames)
                                        .encode('utf-8'))
                outnames = outs.decode('utf-8').split('\n')
                for i in range(0, min(len(cxxnames), len(outnames))):
                    cxxname_maps[cxxnames[i]] = outnames[i]
        except Exception as e:
            print('failed to resolve c++ names: {}'.format(e))

    all_symbols = []
    sections = set()
    files = set()
    for a in all_inputs:
        name, addr, size, section, symscope, symtype, def_file = a
        name = cxxname_maps.get(name, name)
        all_symbols.append({'name': name, 'addr': addr, 'size': size, 'sec': section,
                            'file': filenames[def_file],
                            'symtype': symtype})
        sections.add(section)
        files.add(filenames[def_file])

    sections = sorted(sections)
    files = sorted(files)

    sections = get_options_list(sections)
    files = get_options_list(files)

    fout.write(TMPL.substitute(all_symbols=json.dumps(all_symbols, indent=1),
                               sections='\n'.join(sections),
                               files='\n'.join(files)))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='report elf size')
    parser.add_argument('--elf', type=str, default='',
                        help='path to ELF file')
    parser.add_argument('--symbol_tbl', type=str, default='',
                        help='path to symbol_tbl.txt')
    parser.add_argument('--objsizedump', type=str, default='',
                        help='path to objsizedump')
    parser.add_argument('--cxxfilt', type = str, default = '',
                        help = 'path to c++filt')
    parser.add_argument('--output', type = str, default = 'symbol_tbl.html',
                        help = 'output file')

    args=parser.parse_args(sys.argv[1:])

    if platform.system() == 'Windows':
        if args.cxxfilt == '':
            args.cxxfilt='C:/JL/pi32/bin/llvm-cxxfilt.exe'
        if args.objsizedump == '':
            args.objsizedump='C:/JL/pi32/bin/llvm-objsizedump.exe'
    elif platform.system() == 'Linux':
        if args.cxxfilt == '':
            args.cxxfilt='c++filt'
        if args.objsizedump == '':
            args.objsizedump='/opt/utils/objsizedump'

    with open(args.output, 'w', encoding='utf-8') as fout:

        if args.elf != '':
            with tempfile.TemporaryDirectory() as dirpath:
                symbol_tbl=os.path.join(dirpath, 'symbol_tbl.txt')

                with open(symbol_tbl, 'w', encoding='utf-8') as fsym:
                    try:
                        with subprocess.Popen([args.objsizedump,
                                                '-lite',
                                                '-skip-zero',
                                                '-enable-dbg-info',
                                                args.elf],
                                            stdout = fsym) as p:
                            p.wait()
                    except Exception as e:
                        print('failed to generate symbol_tbl.txt: {}'.format(e))
                        sys.exit(1)
                    fsym.flush()

                # and
                parse_symbol_tbl_txt(fout, symbol_tbl, args.cxxfilt)
                print('file saved to {}'.format(args.output))
        elif args.symbol_tbl != '':
            parse_symbol_tbl_txt(fout, args.symbol_tbl, args.cxxfilt)
            print('file saved to {}'.format(args.output))

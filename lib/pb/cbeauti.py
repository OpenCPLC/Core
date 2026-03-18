# C code beautifier: 2 spaces, converts comments to //, compact

import re
import sys
from pathlib import Path

def beautify_c(code: str) -> str:
  code = convert_block_comments(code)
  lines = code.split('\n')
  result = []
  prev_empty = False
  brace_depth = 0
  in_macro = False
  for line in lines:
    stripped = line.lstrip()
    ends_backslash = line.rstrip().endswith('\\')
    # Preprocessor start
    if stripped.startswith('#') and not in_macro:
      line = fix_preprocessor(line, ends_backslash)
      result.append(line)
      prev_empty = False
      in_macro = ends_backslash
      continue
    # Macro continuation - fix indent but keep internal spacing
    if in_macro:
      line = fix_macro_continuation(line)
      result.append(line)
      in_macro = ends_backslash
      prev_empty = False
      continue
    # Normal line
    line = line.replace('\t', '  ')
    stripped = line.lstrip(' ')
    indent = len(line) - len(stripped)
    new_indent = convert_indent(indent)
    line = ' ' * new_indent + stripped
    line = fix_inline_comment_spacing(line)
    line = line.rstrip()
    for c in stripped:
      if c == '{': brace_depth += 1
      elif c == '}': brace_depth -= 1
    if not line:
      if brace_depth > 0: continue
      if not prev_empty and result:
        result.append('')
        prev_empty = True
      continue
    prev_empty = False
    result.append(line)
  while result and not result[-1]: result.pop()
  while result and not result[0]: result.pop(0)
  return '\n'.join(result) + '\n'

def convert_indent(spaces: int) -> int:
  if spaces == 0: return 0
  if spaces % 4 == 0: return spaces // 2
  return spaces

def fix_macro_continuation(line: str) -> str:
  """Fix indent in macro continuation line"""
  line = line.replace('\t', '  ')
  stripped = line.lstrip(' ')
  indent = len(line) - len(stripped)
  new_indent = convert_indent(indent)
  return ' ' * new_indent + stripped.rstrip()

def fix_preprocessor(line: str, has_continuation: bool) -> str:
  line = line.replace('\t', '  ')
  stripped = line.lstrip()
  stripped = stripped[1:].lstrip()  # remove # and spaces
  match = re.match(r'(\w+)\s*(.*)', stripped)
  if not match: return '#' + stripped.rstrip()
  directive, rest = match.groups()
  if not rest: return '#' + directive
  # Multi-line macro or macro with args - keep internal spacing
  if directive == 'define':
    if has_continuation or ('(' in rest and rest.index('(') < rest.find(' ') if ' ' in rest else '(' in rest):
      return '#define ' + rest.rstrip()
    # Simple define - compact
    parts = rest.split(None, 1)
    if len(parts) == 1: return '#define ' + parts[0]
    return '#define ' + parts[0] + ' ' + parts[1].strip()
  return '#' + directive + ' ' + rest.rstrip()

def fix_inline_comment_spacing(line: str) -> str:
  stripped = line.lstrip()
  if stripped.startswith('//'): return line
  in_string = False
  string_char = None
  for i, c in enumerate(line):
    if c in '"\'':
      if not in_string:
        in_string = True
        string_char = c
      elif c == string_char and (i == 0 or line[i-1] != '\\'):
        in_string = False
    elif not in_string and line[i:i+2] == '//':
      code_part = line[:i].rstrip()
      comment_part = line[i:]
      if code_part: return code_part + '  ' + comment_part
      return line
  return line

def convert_block_comments(code: str) -> str:
  result = []
  i = 0
  in_string = False
  string_char = None
  while i < len(code):
    if code[i] in '"\'':
      if not in_string:
        in_string = True
        string_char = code[i]
      elif code[i] == string_char and (i == 0 or code[i-1] != '\\'):
        in_string = False
      result.append(code[i])
      i += 1
      continue
    if in_string:
      result.append(code[i])
      i += 1
      continue
    if code[i:i+2] == '/*':
      end = code.find('*/', i+2)
      if end == -1:
        result.append(code[i:])
        break
      comment_content = code[i+2:end]
      if is_real_doxygen(code, i, comment_content):
        result.append(code[i:end+2])
        i = end + 2
        continue
      converted = convert_to_line_comments(comment_content, code, i, end)
      if converted:
        result.append(converted)
      i = end + 2
      continue
    result.append(code[i])
    i += 1
  return ''.join(result)

def is_real_doxygen(code: str, pos: int, content: str) -> bool:
  if code[pos:pos+3] == '/*!': return True
  if code[pos:pos+3] == '/**':
    if len(code) > pos+3 and code[pos+3] != '*':
      if is_doxygen_content(content): return True
  return False

def is_doxygen_content(content: str) -> bool:
  doxy_kw = ['@brief', '@param', '@return', '@note', '@warning', '@see',
    '@file', '@def', '@enum', '@struct', '@typedef', '@var',
    '\\brief', '\\param', '\\return', '\\note']
  return any(kw in content for kw in doxy_kw)

def extract_comment_text(content: str) -> list:
  lines = content.split('\n')
  clean_lines = []
  for line in lines:
    clean = line.strip()
    while clean.startswith('*'): clean = clean[1:].lstrip()
    while clean.endswith('*'): clean = clean[:-1].rstrip()
    clean = clean.strip('=-').strip()
    if clean: clean_lines.append(clean)
  return clean_lines

def convert_to_line_comments(content: str, full_code: str, start_pos: int, end_pos: int) -> str:
  line_start = full_code.rfind('\n', 0, start_pos) + 1
  before_comment = full_code[line_start:start_pos]
  code_before = before_comment.strip()
  clean_lines = extract_comment_text(content)
  if not clean_lines: return ''
  if code_before:
    return '  // ' + ' '.join(clean_lines)
  if len(clean_lines) == 1: return '// ' + clean_lines[0]
  next_line_start = full_code.find('\n', end_pos) + 1
  indent_str = ''
  if next_line_start > 0:
    next_line_end = full_code.find('\n', next_line_start)
    if next_line_end == -1: next_line_end = len(full_code)
    next_line = full_code[next_line_start:next_line_end]
    indent_str = ' ' * (len(next_line) - len(next_line.lstrip(' \t')))
  res = ['// ' + clean_lines[0]]
  for clean in clean_lines[1:]:
    res.append(indent_str + '// ' + clean)
  return '\n'.join(res)

def main():
  if len(sys.argv) < 2:
    print(f"Usage: {sys.argv[0]} <file.c> [output.c]")
    sys.exit(1)
  infile = Path(sys.argv[1])
  outfile = Path(sys.argv[2]) if len(sys.argv) > 2 else infile
  code = infile.read_text()
  beautified = beautify_c(code)
  outfile.write_text(beautified)
  print(f"Done: {outfile}")

if __name__ == '__main__':
  main()
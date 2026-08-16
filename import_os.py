

import os

#folder_path = 'c:/Users/lenzre/OneDrive - Konecranes Plc/Home/CXT-DWP/platform-io/T-BEAM-GPS-pingpong/src/'
folder_path = 'c:/Users/lenzre/OneDrive - Konecranes Plc/Home/CXT-DWP/platform-io/extract_Longitude_Latitude--fromUBX-NAV-SAT-signal/src/'

output_file = 'total_program.txt'

def read_text_file(path):
    # Try common encodings first
    for enc in ('utf-8', 'cp1252'):
        try:
            with open(path, 'r', encoding=enc) as f:
                return f.read()
        except UnicodeDecodeError:
            pass

    # Last resort: keep going and replace undecodable bytes
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        return f.read()

with open(output_file, 'w', encoding='utf-8', newline='\n') as outfile:
    for filename in sorted(os.listdir(folder_path)):
        if filename.lower().endswith(('.cpp', '.h')):
            file_path = os.path.join(folder_path, filename)

            outfile.write(f'// Content from {filename}\n')
            outfile.write(read_text_file(file_path))
            outfile.write('\n\n')

print(f"Files combined into {output_file}")
from fusesoc.capi2.generator import Generator
import subprocess
import os

class GenWaivers(Generator):
    def run(self):
        verilator_version = subprocess.check_output(['verilator', '--version']).decode('utf-8').split()[1]

        verilator_major = int(verilator_version.split('.')[0])

        root_dir = self.files_root

        if verilator_major < 5:
            waiver_file = 'verilator-waivers-v4.vlt'
        else:
            waiver_file = 'verilator-waivers-v5.vlt'

        waiver_file = os.path.join(root_dir, 'hw', 'misc', waiver_file)
        
        print('Detected Verilator version: %s. Using waiver file: %s' % (verilator_version, waiver_file))
        
        self.add_files([{ waiver_file: {'file_type': 'vlt'} }])

g = GenWaivers()
g.run()
g.write()

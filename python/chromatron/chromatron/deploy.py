
import os

import yaml

from sapphire.buildtools.firmware_package import data_dir
from chromatron.chromatron import DeviceGroup
from chromatron import code_gen

CONFIG_FILE = os.path.join(data_dir(), 'deploy.yaml')

def get_fx_search_paths():
    try:
        with open(CONFIG_FILE, 'r') as f:
            config = yaml.safe_load(f)

    except FileNotFoundError:
        print(f"Deploy config file not found at: {CONFIG_FILE}")
        print(f"Creating default file.")

        config = {
            'fx_search_paths': []
        }

        with open(CONFIG_FILE, 'w') as f:
            f.write(yaml.dump(config))

    return config['fx_search_paths']

def load_file(filename):
    with open(filename, 'r') as f:
        config = yaml.safe_load(f)

    return config

def deploy(config):
    for name, deployment in config.items():
        print(f'Deploying: {name}')

        try:
            query = deployment['query']

        except KeyError:
            raise KeyError("Missing 'query' section")

        try:
            fx_files = deployment['fx']

            fx_paths = get_fx_search_paths()

        except KeyError:
            fx_files = []

        try:
            deploy_files = deployment['files']

        except KeyError:
            deploy_files = []

        try:
            kv = deployment['kv']

        except KeyError:
            kv = []

        try:
            vm = deployment['vm']

        except KeyError:
            vm = []

        print("Querying devices...")

        group = DeviceGroup(*query)

        if len(group) == 0:
            print("No devices found!")
            return

        print(f'Found {len(group)} devices:')
 
        for device in group.values():
            print(f'\t{device}')


        # search for FX files
        fx_file_locations = {}

        for fx in fx_files:
            if fx.endswith('.fx'):
                fx = fx[:-1 * len('.fx')]

            fx_file_locations[fx] = []

            filename = f'{fx}.fx'

            for path in fx_paths:
                for root, dirs, files in os.walk(path):
                    if filename in files:
                        fx_file = os.path.join(root, filename)

                        fx_file_locations[fx].append(fx_file)

        # print(fx_file_locations)

        for fx_file, locations in fx_file_locations.items():
            if len(locations) == 0:
                print(f'FX file: "{fx_file}" not found in search path!')                

            elif len(locations) > 1:
                print(f'FX file: "{fx_file}" has multiple matches!')                


        for device in group.values():
            print(f'Deploying configuration to: {device.name}')

            if len(deploy_files) > 0:
                print(f'Loading files:')

                for file in deploy_files:
                    print(f'{file}')
                    with open(file, 'rb') as f:
                        data = f.read()

                    device.put_file(file, data)

            if len(fx_file_locations) > 0:
                print(f'Loading FX:')

                for fx_name, locations in fx_file_locations.items():
                    fx_file = locations[0]
                    fx_dir = os.path.split(os.path.abspath(fx_file))[0]
                    print(f'{fx_name} from: {fx_dir}')

                    code = code_gen.compile_script(fx_file, debug_print=False).stream
                    fxb_filename = filename + 'b'
                    bin_filename = os.path.join(fx_dir, fxb_filename)

                    with open(bin_filename, 'wb+') as f:
                        f.write(code)

                    device.put_file(fxb_filename, code)

            if len(kv) > 0:
                print(f'Loading KV:')

                for key, value in kv.items():
                    print(f'{key} = {value}')

                    device.set_key(key, value)

            if len(vm) > 0:
                print(f'Setting VM config:')

                for vm_id in vm:
                    reset = False

                    try:
                        prog = vm[vm_id]['prog']

                        if not prog.endswith('.fxb'):
                            prog += '.fxb'

                        reset = True

                    except KeyError:
                        prog = None

                    try:
                        run = vm[vm_id]['run']

                        reset = True

                    except KeyError:
                        run = None

                    if vm_id == 0:
                        vm_prog = 'vm_prog'
                        vm_run = 'vm_run'
                        vm_reset = 'vm_reset'

                    else:
                        vm_prog = f'vm_prog_{vm_id}'
                        vm_run = f'vm_run_{vm_id}'
                        vm_reset = f'vm_reset_{vm_id}'

                    if prog is not None:
                        device.set_key(vm_prog, prog)

                    if run is not None:
                        device.set_key(vm_run, run)

                    if reset:
                        device.set_key(vm_reset, True)



def deploy_file(filename):
    deploy(load_file(filename))



deploy_file('/home/jeremy/JEREMY/SAPPHIRE/chromatron/python/chromatron/chromatron/deploy_demo.yaml')


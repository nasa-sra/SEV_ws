import sys
if sys.prefix == '/usr':
    sys.real_prefix = sys.prefix
    sys.prefix = sys.exec_prefix = '/home/saath/NASA/SEV/SEV_ws/install/path_recorder'

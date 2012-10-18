from sys import path
import os
path.append(os.getcwd())

from test_validator import validate as test_validate
validators['42'] = 'test_validate'

from test_validator import cleaner as test_cleaner
cleaners['42'] = 'test_cleaner'






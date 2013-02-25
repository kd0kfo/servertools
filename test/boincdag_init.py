from sys import path
import os
path.append(os.getcwd())

if not "validators" in globals():
    validators = {}
if not "cleaners" in globals():
    cleaners = {}
if not "assimilators" in globals():
    assimilators = {}


from test_validator import validate as test_validate
validators['42'] = 'test_validate'

from test_validator import cleaner as test_cleaner
cleaners['42'] = 'test_cleaner'

def sim_assim(results,canonical_result):
    # do nothing
    return

assimilators['42'] = 'sim_assim'



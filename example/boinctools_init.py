from sys import path
import os
import dag.util as dag_utils
path.append(dag_utils.project_path)

from validators.trident_validator import validate as tvalidator
from validators.trident_validator import clean as tcleaner
from validators.trident_validator import assimilator as tassimilator

trident_id = '11'

validators = {}
validators[trident_id] = 'tvalidator'


cleaners = {}
cleaners[trident_id] = 'tcleaner'

validators['42'] = validators[trident_id]
cleaners['42'] = cleaners[trident_id]
cleaners['123'] = cleaners[trident_id]

assimilators = {}
assimilators[trident_id] = 'tassimilator'

from iocbuilder import Device, AutoSubstitution, SetSimulation
from iocbuilder.arginfo import *

from iocbuilder.modules.areaDetector import AreaDetector, _ADBase, _ADBaseTemplate, simDetector

class _aravisCamera(AutoSubstitution):
    TemplateFile="aravisCamera.template"
    SubstitutionOverwrites = [_ADBaseTemplate]
    
class _aravisProsilica(AutoSubstitution):
    TemplateFile="aravisProsilica.template"

class aravisCamera(_ADBase):
    '''Creates a aravisCamera camera areaDetector driver'''
    _SpecificTemplate = _aravisCamera
    def __init__(self, ID, CLASS, BUFFERS = 50, MEMORY = -1, **args):
        # Init the superclass
        self.__super.__init__(**args)
        # Init the camera specific class
        _cameraSpecific = globals()["_aravis%s" % CLASS]
        self.specific = _cameraSpecific(**filter_dict(args, _cameraSpecific.ArgInfo.Names()))
        # Store the args
        self.__dict__.update(locals())

    # __init__ arguments
    ArgInfo = _ADBase.ArgInfo + _aravisCamera.ArgInfo + makeArgInfo(__init__,
        ID      = Simple('Cam ID, <manufacturer>-<serial>, (e.g. Prosilica-02-2166A-06844)', str),    
        CLASS   = Choice('Camera class for custom commands', ["Prosilica"]),
        BUFFERS = Simple('Maximum number of NDArray buffers to be created for '
            'plugin callbacks', int),
        MEMORY  = Simple('Max memory to allocate, should be maxw*maxh*nbuffer '
            'for driver and all attached plugins', int))

    # Device attributes
    LibFileList = ['aravisCamera']
    DbdFileList = ['aravisCameraSupport']

    def Initialise(self):
        print '# aravisCameraConfig(portName, cameraName, ' \
            'maxBuffers, maxMemory)'
        print 'aravisCameraConfig("%(PORT)s", "%(ID)s", ' \
            '%(BUFFERS)d, %(MEMORY)d)' % self.__dict__



def aravisCamera_sim(**kwargs):
    return simDetector(1024, 768, **kwargs)

SetSimulation(aravisCamera, aravisCamera_sim)

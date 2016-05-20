#!/bin/env python
import os, sys
from xml.dom.minidom import parseString
from optparse import OptionParser

# parse args
parser = OptionParser("""%prog <genicam_xml> <camera_name>

This script parses a genicam xml file and creates a database template and edm 
screen to go with it. The edm screen should be used as indication of what
the driver supports, and the generated summary screen should be edited to make
a more sensible summary. The Db file will be generated in:
  ../Db/<camera_name>.template
and the edm files will be called:
  ../opi/edl/<camera_name>.edl
  ../opi/edl/<camera_name>-features.edl""")
options, args = parser.parse_args()
if len(args) != 2:
    parser.error("Incorrect number of arguments")

# parse xml file to dom object
genicam_lines = open(args[0]).readlines()
try:
    start_line = min(i for i in range(2) if genicam_lines[i].lstrip().startswith("<"))
except:
    print "Neither of these lines looks like valid XML:"
    print "".join(genicam_lines[:2])
    sys.exit(1)

xml_root = parseString("".join(genicam_lines[start_line:]).lstrip())
camera_name = args[1]
prefix = os.path.abspath(os.path.join(os.path.dirname(__file__),".."))
db_filename = os.path.join(prefix, "Db", camera_name + ".template")
edl_filename = os.path.join(prefix, "opi", "edl", camera_name + ".edl")
edl_more_filename = os.path.join(prefix, "opi", "edl", camera_name + "-features.edl")

# function to read element children of a node
def elements(node):
    return [n for n in node.childNodes if n.nodeType == n.ELEMENT_NODE]  

# a function to read the text children of a node
def getText(node):
    return ''.join([n.data for n in node.childNodes if n.nodeType == n.TEXT_NODE])

# node lookup from nodeName -> node
lookup = {}
# lookup from nodeName -> recordName
records = {}
categories = []

# function to create a lookup table of nodes
def handle_node(node):
    if node.nodeName == "Group":
        for n in elements(node):
            handle_node(n)
    elif node.hasAttribute("Name"):
        name = str(node.getAttribute("Name"))
        lookup[name] = node
        recordName = name
        if len(recordName) > 16:
            recordName = recordName[:16]
        i = 0
        while recordName in records.values():
            recordName = recordName[:-len(str(i))] + str(i)
            i += 1
        records[name] = recordName
        if node.nodeName == "Category":
            categories.append(name)
    elif node.nodeName != "StructReg":
        print "Node has no Name attribute", node

# list of all nodes    
for node in elements(elements(xml_root)[0]):
    handle_node(node)

# Now make structure, [(title, [features...]), ...]
structure = []
doneNodes = []
def handle_category(category):
    # making flat structure, so if its already there then don't do anything
    if category in [x[0] for x in structure]:
        return
    node = lookup[category]
    # for each child feature of this node
    features = []
    cgs = []
    for feature in elements(node):        
        if feature.nodeName == "pFeature":
            featureName = str(getText(feature))
            featureNode = lookup[featureName]
            if str(featureNode.nodeName) == "Category":
                cgs.append(featureName)
            else:
                if featureNode not in doneNodes:
                    features.append(featureNode)   
                    doneNodes.append(featureNode)
    if features:
        if len(features) > 32:
            i = 1
            while features:
                structure.append((category+str(i), features[:32]))
                i += 1
                features = features[32:]
        else:            
            structure.append((category, features))
    for category in cgs:
        handle_category(category)

for category in categories:
    handle_category(category)
    
# Spit out a database file
db_file = open(db_filename, "w")
stdout = sys.stdout
sys.stdout = db_file

# print a header
print '# Macros:'
print '#% macro, P, Device Prefix'
print '#% macro, R, Device Suffix'
print '#% macro, PORT, Asyn Port name'
print '#% macro, TIMEOUT, Timeout, default=1'
print '#% macro, ADDR, Asyn Port address, default=0'
print '#%% gui, $(PORT), edmtab, %s.edl, P=$(P),R=$(R)' % camera_name
print 

a_autosaveFields		= 'DESC LOLO LOW HIGH HIHI LLSV LSV HSV HHSV EGU TSE PREC'
b_autosaveFields		= 'DESC ZSV OSV TSE'
long_autosaveFields		= 'DESC LOLO LOW HIGH HIHI LLSV LSV HSV HHSV EGU TSE'
mbb_autosaveFields		= 'DESC ZRSV ONSV TWSV THSV FRSV FVSV SXSV SVSV EISV NISV TESV ELSV TVSV TTSV FTSV FFSV TSE'
string_autosaveFields	= 'DESC TSE'

# for each node
for node in doneNodes:
    nodeName = str(node.getAttribute("Name"))
    ro = False
    for n in elements(node):
        if str(n.nodeName) == "AccessMode" and getText(n) == "RO":
            ro = True
    if node.nodeName in ["Integer", "IntConverter", "IntSwissKnife"]:
        print 'record(longin, "$(P)$(R)%s_RBV") {' % records[nodeName]
        print '  field(DTYP, "asynInt32")'
        print '  field(INP,  "@asyn($(PORT),$(ADDR),$(TIMEOUT))ARVI_%s")' % nodeName
        print '  field(SCAN, "I/O Intr")'
        print '  field(DISA, "0")'        
        print '  info( autosaveFields, "%s" )' % long_autosaveFields
        print '}'
        print
        if ro:
            continue        
        print 'record(longout, "$(P)$(R)%s") {' % records[nodeName]
        print '  field(DTYP, "asynInt32")'
        print '  field(OUT,  "@asyn($(PORT),$(ADDR),$(TIMEOUT))ARVI_%s")' % nodeName
        print '  field(DISA, "0")'
        print '  info( autosaveFields, "%s PINI VAL" )' % long_autosaveFields
        print '}'
        print        
    elif node.nodeName in ["Boolean"]:
        print 'record(bi, "$(P)$(R)%s_RBV") {' % records[nodeName]
        print '  field(DTYP, "asynInt32")'
        print '  field(INP,  "@asyn($(PORT),$(ADDR),$(TIMEOUT))ARVI_%s")' % nodeName
        print '  field(SCAN, "I/O Intr")'
        print '  field(ZNAM, "No")'
        print '  field(ONAM, "Yes")'                        
        print '  field(DISA, "0")'
        print '  info( autosaveFields, "%s" )' % b_autosaveFields
        print '}'
        print
        if ro:
            continue        
        print 'record(bo, "$(P)$(R)%s") {' % records[nodeName]
        print '  field(DTYP, "asynInt32")'
        print '  field(OUT,  "@asyn($(PORT),$(ADDR),$(TIMEOUT))ARVI_%s")' % nodeName
        print '  field(ZNAM, "No")'
        print '  field(ONAM, "Yes")'                                
        print '  field(DISA, "0")'
        print '  info( autosaveFields, "%s PINI VAL" )' % b_autosaveFields
        print '}'
        print           
    elif node.nodeName in ["Float", "Converter", "SwissKnife"]:
        print 'record(ai, "$(P)$(R)%s_RBV") {' % records[nodeName]
        print '  field(DTYP, "asynFloat64")'
        print '  field(INP,  "@asyn($(PORT),$(ADDR),$(TIMEOUT))ARVD_%s")' % nodeName
        print '  field(PREC, "3")'        
        print '  field(SCAN, "I/O Intr")'
        print '  field(DISA, "0")'
        print '  info( autosaveFields, "%s" )' % a_autosaveFields
        print '}'
        print    
        if ro:
            continue    
        print 'record(ao, "$(P)$(R)%s") {' % records[nodeName]
        print '  field(DTYP, "asynFloat64")'
        print '  field(OUT,  "@asyn($(PORT),$(ADDR),$(TIMEOUT))ARVD_%s")' % nodeName
        print '  field(PREC, "3")'
        print '  field(DISA, "0")'
        print '  info( autosaveFields, "%s PINI VAL" )' % a_autosaveFields
        print '}'
        print
    elif node.nodeName in ["StringReg"]:
        print 'record(stringin, "$(P)$(R)%s_RBV") {' % records[nodeName]
        print '  field(DTYP, "asynOctetRead")'
        print '  field(INP,  "@asyn($(PORT),$(ADDR),$(TIMEOUT))ARVS_%s")' % nodeName
        print '  field(SCAN, "I/O Intr")'
        print '  field(DISA, "0")'
        print '  info( autosaveFields, "%s" )' % string_autosaveFields
        print '}'
        print
    elif node.nodeName in ["Command"]:
        print 'record(longout, "$(P)$(R)%s") {' % records[nodeName]
        print '  field(DTYP, "asynInt32")'
        print '  field(OUT,  "@asyn($(PORT),$(ADDR),$(TIMEOUT))ARVI_%s")' % nodeName
        print '  field(DISA, "0")'
        print '  info( autosaveFields, "%s" )' % long_autosaveFields
        print '}'
        print         
    elif node.nodeName in ["Enumeration"]:
        enumerations = ""
        i = 0
        epicsId = ["ZR", "ON", "TW", "TH", "FR", "FV", "SX", "SV", "EI", "NI", "TE", "EL", "TV", "TT", "FT", "FF"]
        for n in elements(node):
            if str(n.nodeName) == "EnumEntry":
                if i >= len(epicsId):
                    print >> sys.stderr, "Too many enum entries in %s, truncating" % nodeName
                    break
                name = str(n.getAttribute("Name"))
                enumerations += '  field(%sST, "%s")\n' %(epicsId[i], name[:16])
                value = [x for x in elements(n) if str(x.nodeName) == "Value"]
                assert value, "EnumEntry %s in node %s doesn't have a value" %(name, nodeName)                
                enumerations += '  field(%sVL, "%s")\n' %(epicsId[i], getText(value[0]))                
                i += 1                
        print 'record(mbbi, "$(P)$(R)%s_RBV") {' % records[nodeName]
        print '  field(DTYP, "asynInt32")'
        print '  field(INP,  "@asyn($(PORT),$(ADDR),$(TIMEOUT))ARVI_%s")' % nodeName
        print enumerations,
        print '  field(SCAN, "I/O Intr")'
        print '  field(DISA, "0")'
        print '  info( autosaveFields, "%s" )' % mbb_autosaveFields
        print '}'
        print
        if ro:
            continue        
        print 'record(mbbo, "$(P)$(R)%s") {' % records[nodeName]
        print '  field(DTYP, "asynInt32")'
        print '  field(OUT,  "@asyn($(PORT),$(ADDR),$(TIMEOUT))ARVI_%s")' % nodeName
        print enumerations,       
        print '  field(DISA, "0")'
        print '  info( autosaveFields, "%s PINI VAL" )' % mbb_autosaveFields
        print '}'
        print          
    else:
        print >> sys.stderr, "Don't know what to do with %s" % node.nodeName
    
# tidy up
db_file.close()     
sys.stdout = stdout

# Spit out a feature screen
edl_file = open(edl_more_filename, "w")
w = 260
h = 40
x = 5
y = 50
text = ""

def quoteString(string):
    escape_list = ["\\","{","}",'"']
    for e in escape_list:
        string = string.replace(e,"\\"+e) 
    string = string.replace("\n", "").replace(",", ";")
    return string

def make_box():
    return """# (Rectangle)
object activeRectangleClass
beginObjectProperties
major 4
minor 0
release 0
x %(x)d
y %(y)d
w 255
h %(boxh)d
lineColor index 14
fill
fillColor index 5
endObjectProperties

# (Static Text)
object activeXTextClass
beginObjectProperties
major 4
minor 1
release 0
x %(x)d
y %(laby)d
w 150
h 14
font "arial-medium-r-12.0"
fontAlign "center"
fgColor index 14
bgColor index 8
value {
  "  %(name)s  "
}
autoSize
border
endObjectProperties

""" % globals()

def make_description():
    return """# (Related Display)
object relatedDisplayClass
beginObjectProperties
major 4
minor 2
release 0
x %(nx)d
y %(y)d
w 10
h 20
fgColor index 14
bgColor index 3
topShadowColor index 1
botShadowColor index 11
font "arial-bold-r-10.0"
xPosOffset -100
yPosOffset -85
useFocus
buttonLabel "?"
numPvs 4
numDsps 1
displayFileName {
  0 "aravisHelp"
}
setPosition {
  0 "button"
}
symbols {
  0 "desc0=%(desc0)s,desc1=%(desc1)s,desc2=%(desc2)s,desc3=%(desc3)s,desc4=%(desc4)s,desc5=%(desc5)s"
}
endObjectProperties                

""" % globals()

def make_label():
    return """
# (Static Text)
object activeXTextClass
beginObjectProperties
major 4
minor 1
release 0
x %(nx)d
y %(y)d
w 110
h 20
font "arial-bold-r-10.0"
fgColor index 14
bgColor index 3
useDisplayBg
value {
  "%(nodeName)s"
}
endObjectProperties   

""" % globals()             

def make_ro():
    return """# (Textupdate)
object TextupdateClass
beginObjectProperties
major 10
minor 0
release 0
x %(nx)d
y %(y)d
w 125
h 20
controlPv "$(P)$(R)%(recordName)s_RBV"
fgColor index 16
fgAlarm
bgColor index 10
fill
font "arial-bold-r-12.0"
fontAlign "center"
endObjectProperties        

""" % globals()         

def make_demand():
    return """# (Textentry)
object TextentryClass
beginObjectProperties
major 10
minor 0
release 0
x %(nx)d
y %(y)d
w 60
h 20
controlPv "$(P)$(R)%(recordName)s"
fgColor index 25
fgAlarm
bgColor index 3
fill
font "arial-bold-r-12.0"
endObjectProperties

""" % globals()

def make_rbv():
    return """# (Textupdate)
object TextupdateClass
beginObjectProperties
major 10
minor 0
release 0
x %(nx)d
y %(y)d
w 60
h 20
controlPv "$(P)$(R)%(recordName)s_RBV"
fgColor index 16
fgAlarm
bgColor index 10
fill
font "arial-bold-r-12.0"
fontAlign "center"
endObjectProperties

""" % globals() 

def make_menu():
    return """# (Menu Button)
object activeMenuButtonClass
beginObjectProperties
major 4
minor 0
release 0
x %(nx)d
y %(y)d
w 125
h 20
fgColor index 25
bgColor index 3
inconsistentColor index 0
topShadowColor index 1
botShadowColor index 11
controlPv "$(P)$(R)%(recordName)s"
indicatorPv "$(P)$(R)%(recordName)s_RBV"
font "arial-bold-r-12.0"
endObjectProperties        

""" % globals()

def make_cmd():
    return """# (Message Button)
object activeMessageButtonClass
beginObjectProperties
major 4
minor 0
release 0
x %(nx)d
y %(y)d
w 125
h 20
fgColor index 25
onColor index 3
offColor index 3
topShadowColor index 1
botShadowColor index 11
controlPv "$(P)$(R)%(recordName)s.PROC"
pressValue "1"
onLabel "%(nodeName)s"
offLabel "%(nodeName)s"
3d
font "arial-bold-r-12.0"
endObjectProperties

""" % globals()

# Write each section
for name, nodes in structure:
    # write box
    boxh = len(nodes) * 25 + 5
    if (boxh + y > 850):
        y = 50
        w += 260
        x += 260  
    laby = y - 10      
    text += make_box()
    y += 5
    h = max(y, h)    
    for node in nodes:
        nodeName = str(node.getAttribute("Name"))
        recordName = records[nodeName]
        ro = False
        desc = ""
        for n in elements(node):
            if str(n.nodeName) == "AccessMode" and getText(n) == "RO":
                ro = True
            if str(n.nodeName) in ["ToolTip", "Description"]:
                desc = getText(n)
        descs = ["%s: "% nodeName, "", "", "", "", ""]
        i = 0
        for word in desc.split():
            if len(descs[i]) + len(word) > 80:
                i += 1
                if i >= len(descs):
                    break
            descs[i] += word + " "
        for i in range(6):
            if descs[i]:
                globals()["desc%d" % i] = quoteString(descs[i])
            else:
                globals()["desc%d" % i] = "''"
        nx = x + 5
        text += make_description()   
        nx += 10
        text += make_label()
        nx += 110            
        if node.nodeName in ["StringReg"] or ro:
            text += make_ro()
        elif node.nodeName in ["Integer", "Float", "Converter", "IntConverter", "IntSwissKnife", "SwissKnife"]:  
            text += make_demand()
            nx += 65 
            text += make_rbv() 
        elif node.nodeName in ["Enumeration", "Boolean"]:
            text += make_menu()
        elif node.nodeName in ["Command"]:
            text += make_cmd()
        else:
            print "Don't know what to do with", node.nodeName
        y += 25
    y += 15
    h = max(y, h)    

# tidy up
w += 5
exitX = w - 100
exitY = h - min(30, h - y)
h = exitY + 30
edl_file.write("""4 0 1
beginScreenProperties
major 4
minor 0
release 1
x 50
y 50
w %(w)d
h %(h)d
font "arial-bold-r-12.0"
ctlFont "arial-bold-r-12.0"
btnFont "arial-bold-r-12.0"
fgColor index 14
bgColor index 3
textColor index 14
ctlFgColor1 index 25
ctlFgColor2 index 25
ctlBgColor1 index 3
ctlBgColor2 index 3
topShadowColor index 1
botShadowColor index 11
title "%(camera_name)s features - $(P)$(R)"
showGrid
snapToGrid
gridSize 5
endScreenProperties

# (Group)
object activeGroupClass
beginObjectProperties
major 4
minor 0
release 0
x 0
y 0
w %(w)d
h 30

beginGroup

# (Rectangle)
object activeRectangleClass
beginObjectProperties
major 4
minor 0
release 0
x 0
y 0
w %(w)d
h 30
lineColor index 3
fill
fillColor index 3
endObjectProperties

# (Lines)
object activeLineClass
beginObjectProperties
major 4
minor 0
release 1
x 0
y 2
w %(w)d
h 24
lineColor index 11
fillColor index 0
numPoints 3
xPoints {
  0 0
  1 %(w)d
  2 %(w)d
}
yPoints {
  0 26
  1 26
  2 2
}
endObjectProperties

# (Static Text)
object activeXTextClass
beginObjectProperties
major 4
minor 1
release 0
x 0
y 2
w %(w)d
h 24
font "arial-bold-r-16.0"
fontAlign "center"
fgColor index 14
bgColor index 48
value {
  "%(camera_name)s features - $(P)$(R)"
}
endObjectProperties

# (Lines)
object activeLineClass
beginObjectProperties
major 4
minor 0
release 1
x 0
y 2
w %(w)d
h 24
lineColor index 1
fillColor index 0
numPoints 3
xPoints {
  0 0
  1 0
  2 %(w)d
}
yPoints {
  0 26
  1 2
  2 2
}
endObjectProperties

endGroup

endObjectProperties

""" %globals())
edl_file.write(text.encode('ascii', 'replace'))
edl_file.write("""# (Exit Button)
object activeExitButtonClass
beginObjectProperties
major 4
minor 1
release 0
x %(exitX)d
y %(exitY)d
w 95
h 25
fgColor index 46
bgColor index 3
topShadowColor index 1
botShadowColor index 11
label "EXIT"
font "arial-bold-r-14.0"
3d
endObjectProperties
""" % globals())
edl_file.close()
    
# write the summary screen
if not os.path.exists(edl_filename):
    open(edl_filename, "w").write("""4 0 1
beginScreenProperties
major 4
minor 0
release 1
x 713
y 157
w 390
h 820
font "arial-bold-r-12.0"
ctlFont "arial-bold-r-12.0"
btnFont "arial-bold-r-12.0"
fgColor index 14
bgColor index 3
textColor index 14
ctlFgColor1 index 25
ctlFgColor2 index 25
ctlBgColor1 index 3
ctlBgColor2 index 3
topShadowColor index 1
botShadowColor index 11
showGrid
snapToGrid
gridSize 5
endScreenProperties

# (Rectangle)
object activeRectangleClass
beginObjectProperties
major 4
minor 0
release 0
x 0
y 470
w 390
h 350
lineColor index 5
fill
fillColor index 5
endObjectProperties

# (Embedded Window)
object activePipClass
beginObjectProperties
major 4
minor 1
release 0
x 0
y 0
w 390
h 470
fgColor index 14
bgColor index 3
topShadowColor index 1
botShadowColor index 11
displaySource "file"
file "ADBase"
sizeOfs 5
numDsps 0
noScroll
endObjectProperties

# (Embedded Window)
object activePipClass
beginObjectProperties
major 4
minor 1
release 0
x 0
y 470
w 390
h 110
fgColor index 14
bgColor index 3
topShadowColor index 1
botShadowColor index 11
displaySource "file"
file "aravisCamera"
sizeOfs 5
numDsps 0
noScroll
endObjectProperties

# (Related Display)
object relatedDisplayClass
beginObjectProperties
major 4
minor 2
release 0
x 5
y 790
w 380
h 25
fgColor index 43
bgColor index 3
topShadowColor index 1
botShadowColor index 11
font "arial-bold-r-14.0"
buttonLabel "more features..."
numPvs 4
numDsps 1
displayFileName {
  0 "%s-features"
}
setPosition {
  0 "parentWindow"
}
endObjectProperties""" % camera_name)


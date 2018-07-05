#!/bin/env python
import os, sys
from xml.dom.minidom import parseString
from optparse import OptionParser

# parse args
parser = OptionParser("""%prog <genicam_xml> <camera_name>

This script parses a genicam xml file and creates an medm 
screen to go with it. The medm screen should be used as indication of what
the driver supports, and the generated summary screen should be edited to make
a more sensible summary. 
and the medm files will be called:
  ../op/adl/<camera_name>.adl
  ../op/adl/<camera_name>-features.adl""")
options, args = parser.parse_args()
if len(args) != 2:
    parser.error("Incorrect number of arguments")

# Check the first two lines of the feature xml file to see if arv-tool left
# the camera id there, thus creating an unparsable file
# Throw it away if it doesn't look like valid xml
# A valid first line of an xml file will be optional whitespace followed by '<'
genicam_lines = open(args[0]).readlines()
try:
    start_line = min(i for i in range(2) if genicam_lines[i].lstrip().startswith("<"))
except:
    print "Neither of these lines looks like valid XML:"
    print "".join(genicam_lines[:2])
    sys.exit(1)

# parse xml file to dom object
xml_root = parseString("".join(genicam_lines[start_line:]).lstrip())
camera_name = args[1]
prefix = os.path.abspath(os.path.join(os.path.dirname(__file__),".."))
adl_more_filename = os.path.join(prefix, "op", "adl", camera_name + "-features")

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

def quoteString(string):
    escape_list = ["\\","{","}",'"']
    for e in escape_list:
        string = string.replace(e,"\\"+e) 
    string = string.replace("\n", "").replace(",", ";")
    return string

def make_box():
    return """# (Rectangle)
    rectangle {
    	object {
    		x=%(x)d
    		y=%(y)d
    		width=%(boxWidth)d
    		height=%(boxHeight)d
    	}
    	"basic attribute" {
    		clr=14
    		fill="outline"
    	}
    }
    
    rectangle {
    	object {
    		x=%(headingX)d
    		y=%(headingY)d
    		width=%(headingWidth)d
    		height=%(headingHeight)d
    	}
    	"basic attribute" {
    		clr=2
    	}
    }
    text {
    	object {
    		x=%(headingX)d
    		y=%(headingY)d
    		width=%(headingWidth)d
    		height=%(headingHeight)d
    	}
    	"basic attribute" {
    		clr=54
    	}
    	textix="  %(name)s"
    	align="horiz. centered"
    }
    """ % globals()

def make_description():
    return """# (Related Display)
		"related display" {
			object {
				x=%(nx)d
				y=%(y)d
				width=10
				height=20
			}
			display[0] {
				label="?"
				name="aravisHelp.adl"
				args="desc0=%(desc0)s,desc1=%(desc1)s,desc2=%(desc2)s,desc3=%(desc3)s,desc4=%(desc4)s,desc5=%(desc5)s"
			}
			clr=14
			bclr=51
		}
    """ % globals()

def make_label():
    return """
# (Static Text)
		text {
			object {
				x=%(nx)d
				y=%(y)d
				width=%(labelWidth)d
				height=%(labelHeight)d
			}
			"basic attribute" {
				clr=14
			}
			textix="%(nodeName)s"
			align="horiz. right"
		}
    """ % globals()             

def make_ro():
    return """# (Textupdate)
		"text update" {
			object {
				x=%(nx)d
				y=%(y)d
				width=%(readbackWidth)d
				height=%(readbackHeight)d
			}
			monitor {
				chan="$(P)$(R)%(recordName)s_RBV"
				clr=54
				bclr=4
			}
			align="horiz. left"
			limits {
			}
		}
    """ % globals()         

def make_demand():
    return """# (Textentry)
		"text entry" {
			object {
				x=%(nx)d
				y=%(y)d
				width=60
				height=20
			}
			control {
				chan="$(P)$(R)%(recordName)s"
				clr=14
				bclr=51
			}
			limits {
			}
		}
    """ % globals()

def make_rbv():
    return """# (Textupdate)
		"text update" {
			object {
				x=%(nx)d
				y=%(y)d
				width=%(textEntryWidth)d
				height=%(textEntryHeight)d
			}
			monitor {
				chan="$(P)$(R)%(recordName)s_RBV"
				clr=54
				bclr=4
			}
			align="horiz. left"
			limits {
			}
		}
    """ % globals() 

def make_menu():
    return """# (Menu Button)
		menu {
			object {
				x=%(nx)d
				y=%(y)d
				width=%(menuWidth)d
				height=%(menuHeight)d
			}
			control {
				chan="$(P)$(R)%(recordName)s"
				clr=14
				bclr=51
			}
		}
    """ % globals()

def make_cmd():
    return """# (Message Button)
    "message button" {
    	object {
    		x=%(nx)d
    		y=%(y)d
    		width=%(messageButtonWidth)d
    		height=%(messageButtonHeight)d
    	}
    	control {
    		chan="$(P)$(R)%(recordName)s.PROC"
    		clr=14
    		bclr=51
    	}
    	label="%(nodeName)s"
    	press_msg="1"
    }
    """ % globals()

def write_adl_file(fileName):
    adl_file = open(fileName, "w")
    adl_file.write("""
    file {
    	name="/home/epics/devel/areaDetector-3-3-1/aravisGigE/aravisGigEApp/op/adl/aravisCamera.adl"
    	version=030109
    }
    display {
    	object {
    		x=50
    		y=50
    		width=%(w)d
    		height=%(h)d
    	}
    	clr=14
    	bclr=4
    	cmap=""
    	gridSpacing=5
    	gridOn=0
    	snapToGrid=0
    }
    "color map" {
    	ncolors=65
    	colors {
    		ffffff,
    		ececec,
    		dadada,
    		c8c8c8,
    		bbbbbb,
    		aeaeae,
    		9e9e9e,
    		919191,
    		858585,
    		787878,
    		696969,
    		5a5a5a,
    		464646,
    		2d2d2d,
    		000000,
    		00d800,
    		1ebb00,
    		339900,
    		2d7f00,
    		216c00,
    		fd0000,
    		de1309,
    		be190b,
    		a01207,
    		820400,
    		5893ff,
    		597ee1,
    		4b6ec7,
    		3a5eab,
    		27548d,
    		fbf34a,
    		f9da3c,
    		eeb62b,
    		e19015,
    		cd6100,
    		ffb0ff,
    		d67fe2,
    		ae4ebc,
    		8b1a96,
    		610a75,
    		a4aaff,
    		8793e2,
    		6a73c1,
    		4d52a4,
    		343386,
    		c7bb6d,
    		b79d5c,
    		a47e3c,
    		7d5627,
    		58340f,
    		99ffff,
    		73dfff,
    		4ea5f9,
    		2a63e4,
    		0a00b8,
    		ebf1b5,
    		d4db9d,
    		bbc187,
    		a6a462,
    		8b8239,
    		73ff6b,
    		52da3b,
    		3cb420,
    		289315,
    		1a7309,
    	}
    }

    rectangle {
    	object {
    		x=0
    		y=4
    		width=%(w)d
    		height=25
    	}
    	"basic attribute" {
    		clr=2
    	}
    }
    
		text {
			object {
				x=0
				y=5
				width=%(w)d
				height=24
			}
			"basic attribute" {
				clr=54
			}
			textix="%(camera_name)s Features Screen #%(fileNumber)d - $(P)$(R)"
			align="horiz. centered"
		}
    """ %globals())
    
    adl_file.write(text.encode('ascii', 'replace'))
    adl_file.close()


# Write each section
stdout = sys.stdout
    
# Generate feature screens
maxColumnsPerScreen = 4
screenHeight = 850
headingHeight = 20
labelWidth = 220
maxLabelHeight = 20
readbackWidth = 125
readbackHeight = 20
textEntryWidth = 60
textEntryHeight = 20
menuWidth = 125
menuHeight = 20
messageButtonWidth = 125
messageButtonHeight = 20
boxWidth = 5 + labelWidth + 5 + readbackWidth + 5
w = boxWidth
h = 40
x = 5
y = 40
text = ""

numColumns = 1
fileNumber = 1
for name, nodes in structure:
    # write box
    boxHeight = len(nodes) * 25 + 10 + maxLabelHeight
    if (boxHeight + y > screenHeight):
        y = 40
        numColumns += 1
        if (numColumns > maxColumnsPerScreen):
            w += 10
            write_adl_file(adl_more_filename + "_" + str(fileNumber) + ".adl")
            numColumns = 1
            fileNumber += 1
            text = ""
            w = boxWidth
            x = 5
            h = 40
        else:
            w += boxWidth + 5
            x += boxWidth + 5
    headingY = y + 5
    headingX = x + 5
    headingWidth = boxWidth -10
    text += make_box()
    y += 10 + headingHeight
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
        #text += make_description()   
        #nx += 10
        tlen = len(nodeName) * 10. / labelWidth
        if tlen <= 1.0:
            labelHeight = 20
        elif tlen <= 1.3:
            labelHeight = 16
        elif tlen <= 1.5:
            labelHeight = 14
        elif tlen <= 1.7:
            labelHeight = 12
        else:
            labelHeight = 10
        text += make_label()
        nx += labelWidth + 5
        if node.nodeName in ["StringReg"] or ro:
            text += make_ro()
        elif node.nodeName in ["Integer", "Float", "Converter", "IntConverter", "IntSwissKnife", "SwissKnife"]:  
            text += make_demand()
            nx += textEntryWidth + 5 
            text += make_rbv() 
        elif node.nodeName in ["Enumeration", "Boolean"]:
            text += make_menu()
        elif node.nodeName in ["Command"]:
            text += make_cmd()
        else:
            print "Don't know what to do with", node.nodeName
        y += 25
    y += 5
    h = max(y, h)

w += 10
write_adl_file(adl_more_filename + "_" + str(fileNumber) + ".adl")



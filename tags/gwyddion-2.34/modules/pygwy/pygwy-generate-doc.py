#!/usr/bin/python
# Script for generation of gwy.py dummy file. The file contains empty function bodies with comments.
# Used for generation of documentation by using epydoc.
# Public domain

import re, sys, string
# interesting variables
doc_dirs = string.split("../../app ../../libdraw ../../libgwyddion ../../libgwydgets ../../libgwymodule ../../libprocess .")
defs_file = "pygwy.defs"
wrap_file = "pygwywrap.c"
ignore_functions = ["static", "typedef", "be", "parameter"]
ignore_endswith = ["_wrap"]
datatype = {'gdouble': "float", 'GDoubleValue': "float", 'gboolean': "bool", 'const-gchar*': "string", 'const-guchar*': "string", \
   'gint': "int", 'guint': "int", 'gint32': "int", 'GIntValue': "int", 'GwyRGBA*': "L{RGBA}", 'GQuark': "int", 'GwyContainer*': "L{Container}", \
   'GwyDataField*': "L{DataField}", 'GwyDataLine*': "L{DataLine}", 'GwyGraphModel*': "L{GraphModel}", 'GwySIUnit*': "L{SIUnit}", 'GwyGraph*': "L{Graph}", \
   'GwySpectra*': "L{Spectra}", 'GwyDataView*': "L{DataView}", 'GtkWidget*': "L{gtk.Widget}", 'GObject*': "L{gobject.GObject}", 'GtkWindow*': "L{gtk.Window}", \
   'GtkTooltips*': "L{gtk.Tooltips}", 'GtkAccelGroup*': "L{gtk.AccelGroup}", 'GtkComboBox*': "L{gtk.ComboBox}"}


def get_python_name(c_name):
   py_name = c_name.capitalize()
   letters = list(re.findall("_[a-z]", c_name))
   for match in letters:
      py_name = py_name.replace(match, match.upper()[1])
   return py_name.replace("_", "")

def get_c_name(py_name):
   c_name = py_name
   l = re.findall("[A-Z][A-Z][A-Z]*", py_name)
   for m in l:
      c_name = c_name.replace(m, '_'+m)
   l = re.findall("[A-Z][a-z][a-z]*", py_name)
   for m in l[1:]:
      c_name = c_name.replace(m, '_'+m)
   return c_name.lower()

def rep_code(r):
   return "B{C{"+r.groups()[0]+"}}"

def rep_location(r):
   return "L{"+r.groups()[0]+"}"

def replace_special(s):
   p = re.compile("%([a-zA-Z0-9_]+)")
   s = re.sub(p, rep_code, s)
   p = re.compile("@(?!param)(?!return)([a-zA-Z0-9_]+)")
   s = re.sub(p, rep_code, s)
   p = re.compile("#([a-zA-Z0-9_]+)")
   s = re.sub(p, rep_code, s)
   s = s.replace("TRUE", "True")
   s = s.replace("FALSE", "False")
   p = re.compile("(gwy_[a-zA-Z0-9_]+)")
   s = re.sub(p, rep_location, s)
   for c_class in c_class_list:
      s = s.replace(c_class+"_", get_python_name(c_class)+'.')
   s = s.replace("Gwyddion", "ABCgwyddion")
   s = s.replace("Gwy", "")
   s = s.replace("ABCgwyddion", "Gwyddion")
   s = s.replace("GWY_", "")
   #p = re.compile("(gwy\.[a-zA-Z\._]+)")
   #s = re.sub(p, rep_location, s)

   return s

def format_func(str, spaces, level=1):
   indent = ""
   for i in range(level):
      indent += spaces
   r = re.compile("^\s+", re.M)
   res = re.sub(r, indent+spaces, str, 0) # change indentation
   r = re.compile("^\s+def", re.M)
   res = re.sub(r, indent+"def", res)     # indent def
   return res
 
def printdoc(s):
   # trim fore-spaces
   ptr = re.compile("^\s+")
   s = re.sub(ptr, "", s)

   print replace_special(s)

def replace_datatype(s):   
   if datatype.has_key(s):
      return datatype[s]
   else:
      return s

def print_functions(spaces, method, docs, enums, level=1):
   ignored_params = []
   enum_str = dict()
   spaces2 = spaces
   if level == 0:
      spaces = ""
   print spaces+"def", method.name+"(",

   for i in range(len(method.params)):
      if method.params[i].ptype.startswith("GError"):
         ignored_params.append(method.params[i].pname)
         continue
      if i > 0:
         print ',',
      # from is a keyword in Python
      method.params[i].pname = method.params[i].pname.replace('[', '')
      method.params[i].pname = method.params[i].pname.replace(']', '')
      if method.params[i].pname == "from":
         method.params[i].pname = "_from"
      #if i+1 == len(method.params): 
      print method.params[i].pname,
      #else:
      #   print method.params[i].pname+',',
      for enum in enums:
         if enum.c_name == method.params[i].ptype and docs.has_key(enum.c_name):
            enum_list = ""
            for l in enum.values:
               enum_list += "C{B{"+l[1].replace("GWY_", "") +"}}, "
            # enum_list = docs[enum.c_name].description + enum_list
            enum_str[(method.params[i].pname)] = enum_list
         
   print "):"
   if docs.has_key(method.c_name):
      print spaces+spaces2+"\"\"\""
      printdoc(docs[method.c_name].description),
      for param in docs[method.c_name].params:
         if param[0] in ignored_params:
            continue
         #if level == 1:
         #   continue
         if level == 1 and param == docs[method.c_name].params[0]:
            continue
         if param[0] == "from":
            param_name = "_from"
         else:
            param_name = param[0]
         # find original datatype
         ptype = ""
         for p in method.params:
            if p.pname == param_name:
               ptype = " I{("+ replace_datatype(p.ptype)+")} "
               break
         # check for enum
         if enum_str.has_key(param[0]):
            printdoc('@param '+param_name+': '+param[1].rstrip()+ "Expected values: "+enum_str[param[0]].rstrip()+ptype)
         else:
            printdoc('@param '+param_name+': '+param[1].rstrip()+ptype)
      ret = docs[method.c_name].ret
      if len(ret) > 0:
         if type(ret) is not type(''):
           ret = ret[0]
         printdoc("@return:"+ret)
      print spaces+spaces2+"\"\"\""
   print spaces+spaces2+"return", method.guess_return_value_ownership()

def add_override_methods(parser):
   # add methods and functions from override file
   override_docs = docextract.extract(["../"])
   for d in override_docs.keys():
      is_method = -1
      params = ['parameters']
      for param in override_docs[d].params:
         params.append( ('some_type', param[0]) )
      for i in range(len(c_class_list)):
         if d.startswith(c_class_list[i]+"_"):
            is_method = i
            name = d.replace(c_class_list[i]+"_", "")
            break
      #print ('c-name', d, tuple(params))
      if is_method != -1:
         # add as method
         parser.define_method(name, ('c-name', d), 
                             ('of-object', get_python_name(c_class_list[i])), tuple(params))
      else:
         # add as functions    
         parser.define_function(d, ('c-name', d), tuple(params)) 

def func_cmp(x, y):
   return cmp(x.name, y.name)
   
# Add codegen path to our import path
i = 1
codegendir = None
while i < len(sys.argv):
    arg = sys.argv[i]
    if arg == '--codegendir':
        del sys.argv[i]
        codegendir = sys.argv[i]
        del sys.argv[i]
    elif arg.startswith('--codegendir='):
        codegendir = arg.split('=', 2)[1]
        del sys.argv[i]
    else:
        i += 1
if codegendir:
    sys.path.insert(0, codegendir)
del codegendir

# Load it
import docextract, defsparser, override, definitions, string


docs = docextract.extract(doc_dirs)

p = defsparser.DefsParser(defs_file)

p.startParsing()
# create list of class
c_class_list = []
for obj in p.objects:
   c_class_list.append(get_c_name(obj.c_name))

add_override_methods(p)

f = file(wrap_file)
wrap_file_cont = f.read()
override_file_cont = ""
override = override.Overrides()

#enums
"""
for enum in p.enums:
   if docs.has_key(enum.c_name):
      print "\"\"\""
      printdoc(docs[enum.c_name].description)
      print "\"\"\""
   for i in range(len(enum.values)-1):
      print enum.values[i][1].replace("GWY_", ""), ',',
   print enum.values[len(enum.values)-1][1].replace("GWY_", ""), " = range(", len(enum.values), ")"
"""   

# Keep GENERATED seprarated so that this file is not marked generated.
print "# This is dummy " + "GENERATED" + " file used for generation of documentation"

#objects/classes

for obj in p.objects:
   class_written = False
   if wrap_file_cont.find(obj.name) == -1:
      sys.stderr.write("class "+obj.c_name+" not found.\n")
      continue
   #print dir(obj)
   
   methods = p.find_methods(obj)
   if not methods:
      continue
   constructor = p.find_constructor(obj, override)
   if constructor:
      if not class_written:
         print "class", obj.name+":"
         if docs.has_key(obj.c_name):
            print "     \"\"\""
            printdoc(docs[obj.c_name].name)
            printdoc(docs[obj.c_name].description)
            for param in docs[obj.c_name].params:
               print param
               printdoc(docs[obj.c_name].get_param_description(param))
            printdoc(docs[obj.c_name].ret)
            print "     \"\"\""
            #print dir(docs[obj.c_name])
            #exit()
         class_written = True
      ignore_functions.append(constructor.name)
      constructor.name = "__init__"
      print_functions("     ", constructor, docs, p.enums)
   for method in methods:
      # skip methods beginning with number
      if method.name[0] in ['0', '1','2', '3', '4', '5', '6', '7', '8', '9']:
         method.name = "Num_"+method.name
         continue
      if method.varargs:
         continue
      # skip ignore suffix
      to_ignore = False
      for end in ignore_endswith:
         if method.name.endswith(end):
            to_ignore = True
            break
      if to_ignore:
         continue
         
      if wrap_file_cont.find(method.c_name) == -1 and override_file_cont.find(method.c_name) == -1:
         sys.stderr.write("method "+method.c_name+" not found.\n")
         method.name = "UNIMPLEMENTED_"+method.name
      if not class_written:
         print "class", obj.name+":"         
         if docs.has_key(obj.c_name):
            print "     \"\"\""
            printdoc(docs[obj.c_name].description)
            print "     \"\"\""
         class_written = True
      print_functions("     ", method, docs, p.enums)
   
# functions
p.functions.sort(func_cmp)
for func in p.functions:
   if isinstance(func, definitions.MethodDef):
      continue
   if func.is_constructor_of:
      continue
   # skip functions beginning with number
   if func.name[0] in ['0', '1','2', '3', '4', '5', '6', '7', '8', '9']:
      continue
   # skip varargs functions
   if func.varargs:
      continue
   # skip ignore functions
   if func.name in ignore_functions:
      continue
   # skip ignore suffix
   to_ignore = False
   for end in ignore_endswith:
      if func.name.endswith(end):
         to_ignore = True
         break
   if to_ignore:
      continue
   # check if function is used in pygwywrap.c file
   if wrap_file_cont.find(func.c_name) == -1 and override_file_cont.find(func.c_name):
      sys.stderr.write("Func "+func.c_name+" not found.\n")
      func.name = "z_UNIMPLEMENTED_"+func.name
   print_functions("     ", func, docs, p.enums, 0)


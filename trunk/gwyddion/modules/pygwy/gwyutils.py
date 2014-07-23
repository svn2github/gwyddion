import gwy
def save_dfield_to_png(container, datafield_name, filename, run_type):
   """
   Save desired datafield given by name stored in container to file.

   @param container: gwy.Container which has datafield of given name
   @param datafield_name: datafield name in string representation (like '/0/data')
   @param filename: expected filename
   @run_type: select of interactive (RUN_INTERACTIVE) or noninteractive mode (RUN_NONINTERACTIVE)
   """
   gwy.gwy_app_data_browser_reset_visibility(container, gwy.VISIBILITY_RESET_SHOW_ALL)
   datafield_num = int(datafield_name.split('/')[1])
   gwy.gwy_app_data_browser_select_data_field(container, datafield_num)
   gwy.gwy_file_save(container, filename, run_type)

def get_data_fields(container):
   """
   Get list of available datafield stored in given container

   @param container: gwy.Container with datafields

   @return: a list of datafields
   """
   l = []
   for key in container.keys():
      x = container.get_value(key)
      if type(x) is gwy.DataField:
         l.append(x)

   return l

def get_data_fields_dir(container):
   """
   Get list of available datafield stored in given container as directory where key is key name and value is DataField object.

   @param container: gwy.Container with datafields

   @return: a directory of datafields
   """
   d = {}
   for key in container.keys_by_name():
      x = container.get_value_by_name(key)
      if type(x) is gwy.DataField:
         d[key] = x

   return d

def get_current_datafield():
   """
   Short version of function L{gwy.gwy_app_data_browser_get_current}(gwy.APP_DATA_FIELD)

   @return: current datafield
   """
   return gwy.gwy_app_data_browser_get_current(gwy.APP_DATA_FIELD)

def get_current_container():
   """
   Short version of function L{gwy.gwy_app_data_browser_get_current}(gwy.APP_CONTAINER)

   @return: current container
   """
   return gwy.gwy_app_data_browser_get_current(gwy.APP_CONTAINER)

#def gwy_datafield_get_stats(datafield):
#   d = {}
#   d["average"] = float()
#   d["ra"] = float()
#   d["rms"] = float()
#   d["skew"] = float()
#   d["kurtosis"] = float()
#   datafield.get_stats(d["average"], d["ra"], d["rms"], d["skew"], d["kurtosis"])
#   return d

try:
  import numpy as np
except ImportError: 
  pass
else:
   def data_field_data_as_array(field):
      class gwydfdatap():
         def __init__(self,addr,shape):
            data = (addr,False)
            stride = (8,shape[0]*8)
            self.__array_interface__= { 
               'strides' : stride,
               'shape'   : shape,
               'data'    : data,
               'typestr' : "|f8",
               'version' : 3}
         


      addr = field.get_data_pointer()
      shape = (field.get_xres(),field.get_yres())
      return np.array(gwydfdatap(addr,shape),copy=False)
    
   def brick_data_as_array(brick):
      class gwydfdatap():
         def __init__(self,addr,shape):
            data = (addr,False)
            stride = (8,shape[0]*8,shape[0]*shape[1]*8)
            self.__array_interface__= {
               'strides' : stride,
               'shape'   : shape,
               'data'    : data,
               'typestr' : "|f8",
               'version' : 3}



      addr = brick.get_data_pointer()
      shape = (brick.get_xres(),brick.get_yres(),brick.get_zres())
      return np.array(gwydfdatap(addr,shape),copy=False)

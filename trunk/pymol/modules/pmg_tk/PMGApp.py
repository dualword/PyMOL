#A* -------------------------------------------------------------------
#B* This file contains source code for the PyMOL computer program
#C* copyright 1998-2000 by Warren Lyford Delano of DeLano Scientific. 
#D* -------------------------------------------------------------------
#E* It is unlawful to modify or remove this copyright notice.
#F* -------------------------------------------------------------------
#G* Please see the accompanying LICENSE file for further information. 
#H* -------------------------------------------------------------------
#I* Additional authors of this source file include:
#-*
#-* NOTE: Based on code by John E. Grayson which was in turn 
#-* based on code written by Doug Hellmann. 
#Z* -------------------------------------------------------------------

import sys
from glob import glob
import re
import string
import threading
import traceback
import Queue
import __builtin__

from Tkinter import *
import Pmw

class PMGApp(Pmw.MegaWidget):

   def initOS(self):
       # Initialize platform-specific options
       if sys.platform == 'mac':
          self.initializeTk_mac()
       elif sys.platform[:3] == 'win':
          self.initializeTk_win32()
       elif sys.platform[:5] == 'linux':
          self.initializeTk_unix()
       else:
          self.initializeTk_unix()

       osFrame = { 'win32' : 60, 'irix'  : 41,
                   'darwin': 51, 'cygwin': 60, 
                   'linux' : 31 }
       
       if sys.platform in osFrame.keys():
          self.frameAdjust = osFrame[sys.platform]
       else:
          self.frameAdjust = 51

   def initializeTk_win32(self):
      self.root.option_add('*Font', 'Tahoma 8')
      self.pad = ' '
      
   def initializeTk_mac(self):
      pass
      
   def initializeTk_unix(self):
      pass

   def initializeTk_colors_common(self):
      #self.root.option_add('*background', 'grey')   #let system decide
      self.root.option_add('*foreground', 'black')
      self.root.option_add('*EntryField.Entry.background', 'white')
      self.root.option_add('*Entry.background', 'white')      
      self.root.option_add('*MessageBar.Entry.background', 'gray85')
      self.root.option_add('*Listbox*background', 'white')
      self.root.option_add('*Listbox*selectBackground', 'dark slate blue')
      self.root.option_add('*Listbox*selectForeground', 'white')

   def quit_app(self):
      self.pymol.cmd.log_close()
      self.pymol.cmd.quit()  # avoid logging this - it's inconvenient...

   def flush_fifo_once(self):
      # flush the external GUI fifo command queue
      while not self.fifo.empty():
         try:
            cmmd = self.fifo.get(0)
            exec cmmd
         except:
            traceback.print_exc()
      
   def flush_fifo(self):
      self.flush_fifo_once()
      self.root.after(20,self.flush_fifo) # 50X a second
      
   def run(self):
      # this call to mainloop needs to be replaced with something revocable
      self.flush_fifo_once()
      self.root.mainloop()
      self.quit_app()

   def execute(self,cmmd): 
      self.fifo.put(cmmd)

   def initialize_plugins(self):
      startup_pattern = re.sub(r"[\/\\][^\/\\]*$","/startup/*.py*",__file__)
      # startup_pattern = os.environ['PYMOL_PATH']+"/modules/pmg_tk/startup/*.py*"
      raw_list = glob(startup_pattern)
      unique = {}
      for a in raw_list:
         unique[re.sub(r".*[\/\\]|\.py.*$","",a)] = 1
      for name in unique.keys():
         try:
            if name != "__init__":
               module_context = string.join(string.split(__name__,'.')[0:-1])
               mod_name = module_context+".startup."+name
               __builtin__.__import__(mod_name)
               mod = sys.modules[mod_name]
               if hasattr(mod,'__init__'):
                  mod.__init__(self)
         except:
            suppress = 0
            # suppress error reporting when using old versions of Python
            if float(sys.version[0:3])<2.3:
               if( name in ['apbs_tools' ]):
                  suppress = 1
            if not suppress:
               print "Exception in plugin '%s' -- Traceback follows..."%name
               traceback.print_exc()
               print "Error: unable to initialize plugin '%s'."%name
               
   def set_skin(self,skin):
      self.skin = skin
   
   def __init__(self, pymol_instance):

      self.pymol = pymol_instance
      
      if self.pymol._ext_gui != None:
      
         raise RuntimeError  # only one PMGApp should ever be instantiated
      
      else:

         # create a pymol global so that PyMOL can find the external GUI

         self.pymol._ext_gui = self

         self.skin = None
         
         # initialize Tcl/Tk

         self.root = Tk() # creates the root window for the application

         # color scheme

         self.initializeTk_colors_common()

          # operating-system dependencies

         self.initOS()

         # Python megawigit initialization

         Pmw.initialise(self.root)

         # Initialize the base class
         Pmw.MegaWidget.__init__(self, parent=self.root)

         # read the command line arguments regarding:
         # - the size of the root window
         # - the skin to use
         
         inv = sys.modules.get("pymol.invocation",None)
         if inv!=None:
            self.frameWidth = inv.options.win_x + 220
            self.frameXPos = inv.options.win_px
            self.frameHeight = inv.options.ext_y
            self.frameYPos = inv.options.win_py - (
                   self.frameHeight + self.frameAdjust)
            module_path = inv.options.gui +".skins."+ inv.options.skin
            __import__(inv.options.gui +".skins."+ inv.options.skin)
            sys.modules[module_path].__init__(self)
            
         # define the size of the root window
         
         self.root.geometry('%dx%d+%d+%d' % (
            self.frameWidth, self.frameHeight, self.frameXPos, self.frameYPos))


         # create a FIFO so that PyMOL can send code to be executed by the GUI thread
         
         self.fifo = Queue.Queue(0)
         
         # activate polling on the fifo
         
         self.root.after(1000,self.flush_fifo)

         # and let 'er rip

         if self.skin != None:
            self.skin.setup()



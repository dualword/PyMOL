# 
# basic run through the PDB with wire and cartoon display
#
      
from glob import glob

import threading
import time
import whrandom
import math
from pymol import cmd
import sys, os, os.path

ent_dir = "pdb"

def adjust(delay):
   global cycle_time
   cycle_time = delay 

cmd.set_key("F1",adjust,(0,))
cmd.set_key("F2",adjust,(1,))
cmd.set_key("F3",adjust,(2,))
cmd.set_key("F4",adjust,(3,))
cmd.set_key("F5",adjust,(7,))
cmd.set_key("F6",adjust,(10,))
cmd.set_key("F7",adjust,(15,))
cmd.set_key("F8",adjust,(30,))
cmd.set_key("F9",adjust,(60,))
cmd.set_key("F10",adjust,(120,))

cycle_time = 2.0

cmd.set("line_width","2")
cmd.set("cartoon_tube_radius","0.2")
def load():
   list = glob("pdb/*/*") 
   list = map(lambda x:(whrandom.random(),x),list)
   list.sort() 
   list = map(lambda x:x[1],list)
   l = len(list)
   c = 0
   for file in list:
      c = c + 1
      try:
         cmd.set("suspend_updates","1")  
         cmd.delete('pdb')
         cmd.load(file,'pdb')
         print file
#      cmd.refresh()
#      cmd.hide()
         cmd.show('cartoon')
         cmd.color('red','ss h')
         cmd.color('yellow','ss s')
         cmd.orient('pdb')
         cmd.move('z',-100.0)
         sys.__stderr__.write(".")
         sys.__stderr__.flush()
         n = cmd.count_states()
      finally:
         cmd.set("suspend_updates","0")
      if cmd.count_atoms():
         start=time.time()
         if n>1:
            while (time.time()-start)<cycle_time:
               for a in range(1,n+1):
                  cmd.refresh()
                  cmd.frame(a)
                  cmd.move('z',2)
                  cmd.turn('y',1)
                  time.sleep(0.025)
            sys.__stderr__.write(" %d of %d"%(c,l))
            sys.__stderr__.write("\n")
            sys.__stderr__.flush()
         else:
            cmd.refresh()
            while (time.time()-start)<cycle_time:
               for a in range(1,n+1):
                  time.sleep(0.05)
                  cmd.move('z',1.0)
                  cmd.turn('y',1)
load()



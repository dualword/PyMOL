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
#-* 
#-*
#Z* -------------------------------------------------------------------

if __name__=='pymol.querying':

   import selector

   import cmd
   from cmd import _cmd,lock,unlock,Shortcut,QuietException
   from cmd import _feedback,fb_module,fb_mask

   def get_title(object,state,quiet=1):
      '''
DESCRIPTION

   "get_title" retrieves a text string to the state of a particular
   object which will be displayed when the state is active.

USAGE

   set_title object,state

PYMOL API

   cmd.set_title(string object,int state,string text)

   '''
      r = None
      try:
         lock()
         r = _cmd.get_title(str(object),int(state)-1)
         if not quiet:
            if r!=None:
               print " get_title: %s"%r      
      finally:
         unlock()
      return r

   def transform_object(name,matrix,state=0,log=0,sele=''):
      r = None
      try:
         lock()
         r = _cmd.transform_object(str(name),int(state)-1,list(matrix),int(log),str(sele))
      finally:
         unlock()
      return r

   def translate_atom(sele1,v0,v1,v2,state=0,mode=0,log=0):
      r = None
      sele1 = selector.process(sele1)
      try:
         lock()
         r = _cmd.translate_atom(str(sele1),float(v0),float(v1),float(v2),int(state)-1,int(mode),int(log))
      finally:
         unlock()
      return r

   def distance(name=None,selection1="(lb)",selection2="(rb)",cutoff=None,
                mode=None,zoom=0,width=None,length=None,gap=None,labels=1,quiet=1):
      '''
DESCRIPTION

   "distance" creates a new distance object between two
   selections.  It will display all distances within the cutoff.

USAGE

   distance 
   distance (selection1), (selection2)
   distance name = (selection1), (selection1) [,cutoff [,mode] ]

   name = name of distance object 
   selection1,selection2 = atom selections
   cutoff = maximum distance to display
   mode = 0 (default)

PYMOL API

   cmd.distance( string name, string selection1, string selection2,
          string cutoff, string mode )
   returns the average distance between all atoms/frames

NOTES

   The distance wizard makes measuring distances easier than using
   the "dist" command for real-time operations.

   "dist" alone will show distances between selections (lb) and (rb)
   created by left and right button atom picks.  CTRL-SHIFT/left-click
   on the first atom,  CTRL-SHIFT/right-click on the second, then run
   "dist".

   '''
      # handle unnamed distance 
      r = 1
      if name!=None:
         if len(name):
            if name[0]=='(' or ' ' in name or '/' in name: # we're one argument off...
               if cutoff!=None:
                  mode = cutoff
               if selection2!="(rb)":
                  cutoff = selection2
               if selection1!="(lb)":
                  selection2 = selection1
               selection1=name
               name = None

      if selection1=="(lb)":
         if "lb" not in cmd.get_names('selections'):
            if _feedback(fb_module.cmd,fb_mask.errors):
               print "cmd-Error: The 'lb' selection is undefined."
            r = 0
      if selection2=="(rb)":
         if "rb" not in cmd.get_names('selections'):
            if _feedback(fb_module.cmd,fb_mask.errors):         
               print "cmd-Error: The 'rb' selection is undefined."
            r = 0
      if r:         
         save=cmd.get_setting_legacy('auto_zoom')
         cmd.set('auto_zoom',zoom,quiet=1)

         # if unlabeled, then get next name in series

         if name!=None:
            nam=name
         else:
            try:
               lock()
               cnt = _cmd.get("dist_counter") + 1.0
               _cmd.legacy_set("dist_counter","%1.0f" % cnt)
               nam = "dist%02.0f" % cnt
            finally:
               unlock()

         # defaults
         if mode == None:
            mode = 0
         if cutoff == None:
            cutoff = -1.0
         # preprocess selections
         selection1 = selector.process(selection1)
         selection2 = selector.process(selection2)
         # now do the deed
         try:
            lock()
            if selection2!="same":
               selection2 = "("+selection2+")"
            r = _cmd.dist(str(nam),"("+str(selection1)+")",
                          str(selection2),int(mode),float(cutoff),
                          int(labels),int(quiet))
            if width!=None:
               cmd.set("dash_width",width,nam)
            if length!=None:
               cmd.set("dash_length",length,nam)
            if gap!=None:
               cmd.set("dash_gap",gap,nam)
         finally:
            unlock()
         cmd.set('auto_zoom',save,quiet=1)
      if r<0.0:
         if cmd._raising(): raise QuietException
      return r

   # LEGACY support for cmd.dist
   def dist(*arg,**kw):
      return apply(distance,arg,kw)

   def get_povray():
      '''
DESCRIPTION

   "get_povray" returns a tuple corresponding to strings for a PovRay
   input file.

PYMOL API

   cmd.get_povray()

      '''
      r=None
      try:
         lock()   
         r = _cmd.get_povray()
      finally:
         unlock()
      if not r:
         if cmd._raising(): raise QuietException
      return r

   def count_states(selection="(all)",quiet=1):
      '''
DESCRIPTION

   "count_states" is an API-only function which returns the number of
   states in the selection.

PYMOL API

   cmd.count_states(string selection="(all)")

SEE ALSO

   frame
   '''
      # preprocess selection
      selection = selector.process(selection)
      #
      r = -1
      try:
         lock()
         r = _cmd.count_states(selection)
      finally:
         unlock()
      if r<0:
         if cmd._raising(): raise QuietException
      elif not quiet: print " cmd.count_states: %d states."%r            
      return r

   def count_frames(quiet=1):
      '''
DESCRIPTION

   "count_frames" is an API-only function which returns the number of
   frames defined for the PyMOL movie.

PYMOL API

   cmd.count_frames()

SEE ALSO

   frame, count_states
   '''
      r = -1
      try:
         lock()
         r = _cmd.count_frames()
         if not quiet: print " cmd.count_frames: %d frames"%r      
      finally:
         unlock()
      if r<0:
         if cmd._raising(): raise QuietException
      return r

   def export_dots(object,state):  
   # UNSUPPORTED
      try:
         lock()
         r = _cmd.export_dots(object,int(state)-1)
      finally:
         unlock()
      return r

   def overlap(selection1,selection2,state1=1,state2=1,adjust=0.0,quiet=1):
   #
   #   UNSUPPORTED FEATURE - LIKELY TO CHANGE
   #   (for maximum efficiency, use smaller molecule as selection 1)
   #
      # preprocess selections
      selection1 = selector.process(selection1)
      selection2 = selector.process(selection2)
      #
      r = 1
      try:
         lock()
         r = _cmd.overlap(str(selection1),str(selection2),
                          int(state1)-1,int(state2)-1,
                          float(adjust))
         if not quiet: print " cmd.overlap: %5.3f Angstroms."%r
      finally:
         unlock()
      return r

   def get_color_tuple(name):
      r = None
      try:
         lock()
         r = _cmd.get_color(name,0)
         if r==None:
            if _feedback(fb_module.cmd,fb_mask.errors):         
               print "cmd-Error: Unknown color '%s'."%name
      finally:
         unlock()
      if not r:
         if cmd._raising(): raise QuietException
      return r

   def get_color_indices():
      r = None
      try:
         lock()
         r = _cmd.get_color('',1)
      finally:
         unlock()
      if not r:
         if cmd._raising(): raise QuietException
      return r

   def get_renderer():  # 
      try:
         lock()
         r = _cmd.get_renderer()
      finally:
         unlock()
      return r

   def get_phipsi(selection="(name ca)",state=-1):
      # preprocess selections
      selection = selector.process(selection)
      #   
      r = None
      try:
         lock()
         r = _cmd.get_phipsi("("+str(selection)+")",int(state)-1)
      finally:
         unlock()
      if not r:
         if cmd._raising(): raise QuietException
      return r

   def get_position(quiet=1):
      r = None
      try:
         lock()
         r = _cmd.get_position()
      finally:
         unlock()
      if r==None:
         if cmd._raising(): raise QuietException
      elif not quiet:
         print " cmd.get_position: [%8.3f,%8.3f,%8.3f]"%(r[0],r[1],r[2])
      return r

   def get_dihedral(atom1,atom2,atom3,atom4,state=0,quiet=1):
      '''
DESCRIPTION

   "get_dihedral" returns the dihedral angle between four atoms.  By
   default, the coordinates used are from the current state, however
   an alternate state identifier can be provided.

   By convention, positive dihedral angles are right-handed
   (looking down the atom2-atom3 axis).

USAGE

   get_dihedral atom1, atom2, atom3, atom4 [,state ]

EXAMPLES

   get_dihedral 4/n,4/c,4/ca,4/cb
   get_dihedral 4/n,4/c,4/ca,4/cb,state=4

PYMOL API

   cmd.get_dihedral(atom1,atom2,atom3,atom4,state=0)

      '''
      # preprocess selections
      atom1 = selector.process(atom1)
      atom2 = selector.process(atom2)
      atom3 = selector.process(atom3)
      atom4 = selector.process(atom4)
      #   
      r = None
      try:
         lock()
         r = _cmd.get_dihe(str(atom1),str(atom2),str(atom3),str(atom4),int(state)-1)
      finally:
         unlock()
      if r==None:
         if cmd._raising(): raise QuietException
      elif not quiet:
         print " cmd.get_dihedral: %5.3f degrees."%r
      return r

   def get_model(selection="(all)",state=1):
      '''
DESCRIPTION

   "get_model" returns a ChemPy "Indexed" format model from a selection.

PYMOL API

   cmd.get_model(string selection [,int state] )

      '''
      # preprocess selection
      selection = selector.process(selection)
      #   
      r = None
      try:
         lock()
         r = _cmd.get_model("("+str(selection)+")",int(state)-1)
      finally:
         unlock()
      if r==None:
         if cmd._raising(): raise QuietException
      return r

   def get_area(selection="(all)",state=1,load_b=0,quiet=1):
      '''
      PRE-RELEASE functionality - API will change
      '''
      # preprocess selection
      selection = selector.process(selection)
      #
      r = -1.0
      try:
         lock()
         r = _cmd.get_area("("+str(selection)+")",int(state)-1,int(load_b))
      finally:
         unlock()
      if r<0.0:
         if cmd._raising(): raise QuietException
      elif not quiet: print " cmd.get_area: %5.3f Angstroms^2."%r
      return r

   def get_chains(selection="(all)",state=0,quiet=1):
      '''
      PRE-RELEASE functionality - API will change

      state is currently ignored
      '''
      # preprocess selection
      selection = selector.process(selection)
      #
      r = None
      try:
         lock()
         r = _cmd.get_chains("("+str(selection)+")",int(state)-1)
      finally:
         unlock()
      if r==None:
         if cmd._raising(): raise QuietException
      elif not quiet: print " cmd.get_chains: ",str(r)
      return r


   def get_names(type='objects'):
      '''
DESCRIPTION

   "get_names" returns a list of object and/or selection names.

PYMOL API

   cmd.get_names( [string: "objects"|"selections"|"all"] )

NOTES

   The default behavior is to return only object names.

SEE ALSO

   get_type, count_atoms, count_states
      '''
      mode = 1
      if type=='objects':
         mode = 1
      elif type=='selections':
         mode = 2
      elif type=='all':
         mode = 0
      elif type=='public':
         mode = 3
      try:
         lock()
         r = _cmd.get_names(int(mode))
      finally:
         unlock()
      return r

   def get_type(name,quiet=1):
      '''
DESCRIPTION

   "get_type" returns a string describing the named object or
    selection or the string "nonexistent" if the name in unknown.

PYMOL API

   cmd.get_type(string object-name)

NOTES

   Possible return values are

   "object:molecule"
   "object:map"
   "object:mesh"
   "object:distance"
   "selection"

SEE ALSO

   get_names
      '''
      r = None
      try:
         lock()
         r = _cmd.get_type(str(name))
      finally:
         unlock()
      if not r:
         if _feedback(fb_module.cmd,fb_mask.errors):      
            print "cmd-Error: unrecognized name."
         if cmd._raising(): raise QuietException
      elif not quiet:
         print r
      return r

   def id_atom(selection,mode=0,quiet=1):
      '''
DESCRIPTION

   "id_atom" returns the original source id of a single atom, or
   raises and exception if the atom does not exist or if the selection
   corresponds to multiple atoms.

PYMOL API

   list = cmd.id_atom(string selection)
      '''
      r = -1
      selection = str(selection)
      l = apply(identify,(selection,mode,1))
      ll = len(l)
      if not ll:
         if _feedback(fb_module.cmd,fb_mask.errors):
            print "cmd-Error: atom %s not found by id_atom." % selection
         if cmd._raising(): raise QuietException
      elif ll>1:
         if _feedback(fb_module.cmd,fb_mask.errors):
            print "cmd-Error: multiple atoms %s found by id_atom." % selection
         if cmd._raising(): raise QuietException
      else:
         r = l[0]
         if not quiet:
            if mode:
               print " cmd.id_atom: (%s and id %d)"%(r[0],r[1])
            else:
               print " cmd.id_atom: (id %d)"%r
      return r

   def identify(selection="(all)",mode=0,quiet=1):
      '''
DESCRIPTION

   "identify" returns a list of atom IDs corresponding to the ID code
   of atoms in the selection.

PYMOL API

   list = cmd.identify(string selection="(all)",int mode=0)

NOTES

   mode 0: only return a list of identifiers (default)
   mode 1: return a list of tuples of the object name and the identifier

      '''
      # preprocess selection
      selection = selector.process(selection)
      #      
      r = []
      try:
         lock()
         r = _cmd.identify("("+str(selection)+")",int(mode)) # 0 = default mode
      finally:
         unlock()
      if len(r):
         if not quiet:
            if mode:
               for a in r:
                  print " cmd.identify: (%s and id %d)"%(a[0],a[1])
            else:
               for a in r:
                  print " cmd.identify: (id %d)"%a
      return r

   def index(selection="(all)",quiet=1):
      '''
DESCRIPTION

   "index" returns a list of tuples corresponding to the
   object name and index of the atoms in the selection.

PYMOL API

   list = cmd.index(string selection="(all)")

NOTE

  Atom indices are fragile and will change as atoms are added
  or deleted.  Whenever possible, use integral atom identifiers
  instead of indices.

      '''
      # preprocess selection
      selection = selector.process(selection)
      #      
      r = []
      try:
         lock()
         r = _cmd.index("("+str(selection)+")",0) # 0 = default mode
      finally:
         unlock()
      if not quiet:
         if r:
            for a in r:
               print " cmd.index: (%s`%d)"%(a[0],a[1])
      return r

   def find_pairs(selection1,selection2,state1=1,state2=1,cutoff=3.5,mode=0,angle=45):
      '''
DESCRIPTION

   "find_pairs" is currently undocumented.

      '''
      # preprocess selection
      selection1 = selector.process(selection1)
      selection2 = selector.process(selection2)
      #      
      r = []
      try:
         lock()
         r = _cmd.find_pairs("("+str(selection1)+")",
                             "("+str(selection2)+")",
                             int(state1)-1,int(state2)-1,
                             int(mode),float(cutoff),float(angle))
         # 0 = default mode
      finally:
         unlock()
      return r

   def get_extent(selection="(all)",state=0,quiet=1):
      '''
DESCRIPTION

   "get_extent" returns the minimum and maximum XYZ coordinates of a
   selection as an array:
    [ [ min-X , min-Y , min-Z ],[ max-X, max-Y , max-Z ]]

PYMOL API

   cmd.get_extent(string selection="(all)", state=0 )

      '''
      # preprocess selection
      selection = selector.process(selection)
      #      
      r = None
      try:
         lock()
         r = _cmd.get_min_max(str(selection),int(state)-1)
      finally:
         unlock()
      if not r:
         if cmd._raising(): raise QuietException
      elif not quiet:
         print " cmd.extent: min: [%8.3f,%8.3f,%8.3f]"%(r[0][0],r[0][1],r[0][2])
         print " cmd.extent: max: [%8.3f,%8.3f,%8.3f]"%(r[1][0],r[1][1],r[1][2])      
      return r

   def phi_psi(selection="(byres pk1)"):
      result = cmd.get_phipsi(selection)
      if result!=None:
         kees = result.keys()
         kees.sort()
         cmd.feedback('push')
         cmd.feedback('disable','executive','actions')
         for a in kees:
            cmd.iterate("(%s`%d)"%a,"print ' %-9s "+("( %6.1f, %6.1f )"%result[a])+"'%(resn+'-'+resi+':')")
         cmd.feedback('pop')
      elif _feedback(fb_module.cmd,fb_mask.errors):      
         print "cmd-Error: can't compute phi_psi"
      return result


   def count_atoms(selection="(all)",quiet=1):
      '''
DESCRIPTION

   "count_atoms" returns a count of atoms in a selection.

USAGE

   count_atoms (selection)

PYMOL API

   cmd.count(string selection)

      '''
      # preprocess selection
      selection = selector.process(selection)
      #
      try:
         lock()   
         r = _cmd.select("_count_tmp","("+str(selection)+")",1)
         _cmd.delete("_count_tmp")
      finally:
         unlock()
      if not quiet: print " count_atoms: %d atoms"%r
      return r

   def get_names_of_type(type):
      obj = cmd.get_names('objects')
      types = map(get_type,obj)
      mix = map(None,obj,types)
      lst = []
      for a in mix:
         if a[1]==type:
            lst.append(a[0])
      return lst






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

# parser2.py
# An improved command parser for PyMOL

# === Goals:
#  1. improved 1to1 mapping between pymol "cmd" API and command language
#  2. support for named arguments
#  3. support for calling arbitrary python functions via this mapping

# === Syntatic Examples

# * simple commands
# command

# * commands with arguments
#            
# command value1
# command value1,value2
# command value1,value2,value3

# * commands with named arguments
#
# command argument1=value1
# command argument1=value1,argument2=value2,argument3=value3
# command argument3=value1,argument2=value2,argument1=value1   

# * mixed...
#
# command value1,argument3=value3,arg

# * commands with legacy '=' support for first argument
#
# command string1=value1    
# * which should map to
# command string1,value1

# * legacy '=' combined as above
#
# command string1=value1,value2
# command string1=value1,value2,value3,...
# command string1=value1,argument3=value3

# === Burdens placed on API functions...
#
# None. However, function must have real arguments for error checking.
# 

import re
import string
import sys
import threading

class QuietException:
   def __init__(self,args=None):
      self.args = args

# constants for keyword modes

SIMPLE      = 0  # original pymol parsing (deprecated)
SINGLE      = 1  # arguments
RUN         = 2  # run command 
SPAWN       = 3  # for spawn and fork commands
ABORT       = 4  # terminates command script
NO_CHECK    = 10 # no error checking 
STRICT      = 11 # strict name->argument checking
LEGACY      = 12 # support legacy construct str1=val1,... -> str1,val1,...
LITERAL     = 20 # argument is to be treated as a literal string 
LITERAL1    = 21 # one regular argument, followed by literal string
LITERAL2    = 22 # two regular argument, followed by literal string

# key regular expressions

command_re = re.compile(r"[^\s]+")
whitesp_re = re.compile(r"\s+")
comma_re = re.compile(r"\s*,\s*")
arg_name_re = re.compile(r"[A-Za-z0-9_]+\s*\=")
arg_sele_re = re.compile(r"\(.*\)")
# NOTE '''sdf'sdfs''' doesn't work in below.
arg_value_re = re.compile(r"'''[^']*'''|'[^']*'|"+r'"[^"]*"|[^,;]+')

def trim_sele(st): # utility routine, returns selection string
   pc = 1
   l = len(st)
   c = 1
   while c<l:
      if st[c]=='(':
         pc = pc + 1
      if st[c]==')':
         pc = pc - 1
      c = c + 1
      if not pc:
         break
   if pc:
      return None
   return st[0:c]

def parse_arg(st,mode=STRICT):
   '''
parse_arg(st)

expects entire command to be passed in

returns list of tuples of strings: [(None,value),(name,value)...]
'''
   result = [] 
   # current character
   cc = 0
   mo = command_re.match(st[cc:])
   if mo:
      cc=cc+mo.end(0)
      while 1:
         if mode>=LITERAL: # LITERAL argument handling
            if (mode-LITERAL)==len(result):
               result.append((None,string.strip(st[cc:])))
               return result
         # clean whitespace
         mo = whitesp_re.match(st[cc:])
         if mo:
            cc=cc+mo.end(0)            
         if not len(st[cc:]):
            break
         # read argument name, if any         
         mo = arg_name_re.match(st[cc:])
         if mo:
            nam = string.strip(mo.group(0)[:-1])
            cc=cc+mo.end(0)
         else:
            nam = None
         # clean whitespace
         mo = whitesp_re.match(st[cc:])
         if mo:
            cc=cc+mo.end(0)
         # special handling for un-quoted atom selections
         mo = arg_sele_re.match(st[cc:])
         if mo:
            se = trim_sele(mo.group(0))
            if se==None:
               print "Error: "+st
               print "Error: "+" "*cc+"^ syntax error."
               raise QuietException
            else:
               result.append((nam,se))
               cc = cc + len(se)
         else:
            # read normal argument value
            mo = arg_value_re.match(st[cc:])
            if not mo:
               print "Error: "+st
               print "Error: "+" "*cc+"^ syntax error."
               raise QuietException
            result.append((nam,string.strip(mo.group(0))))
            cc=cc+mo.end(0)
         # clean whitespace
         mo = whitesp_re.match(st[cc:])
         if mo:
            cc=cc+mo.end(0)
         # skip over comma
         if len(st[cc:]):
            mo = comma_re.match(st[cc:])
            if mo:
               cc=cc+mo.end(0)
            else:
               print "Error: "+st
               print "Error: "+" "*cc+"^ syntax error."
               raise QuietException
   return result

def dump_arg(name,arg_lst,nreq):
   ac = 0
   pc = 0
   print "Usage:",name,
   for a in arg_lst:
      if ac>=nreq:
         print "[",
         pc = pc + 1
      if ac:
         print ","+a,
      else:
         print a,
      ac = ac + 1
   print "]"*pc
   
def prepare_call(fn,lst,mode=STRICT,name=None): # returns tuple of arg,kw or excepts if error
   if name==None:
      name=fn.__name__
   result = (None,None)
   arg = []
   kw = {}
   co = fn.func_code
   if (co.co_flags & 0xC): # disable error checking for *arg or **kw functions
      mode = NO_CHECK
   arg_nam = co.co_varnames[0:co.co_argcount]
   narg = len(arg_nam)
   if fn.func_defaults:
      ndef = len(fn.func_defaults)
   else:
      ndef = 0
   nreq = narg-ndef
   if mode==NO_CHECK:
      # no error checking
      for a in lst:
         if a[0]==None:
            arg.append(a[1])
         else:
            kw[a[0]]=a[1]
   else:
      # error checking enabled

      # build name dictionary, with required flag
      arg_dct={}
      c = 0
      for a in arg_nam:
         arg_dct[a]=c<nreq
         c = c + 1
      if mode==LEGACY:
         # handle legacy string=value transformation
         tmp_lst = []
         for a in lst:
            if(a[0]!=None):
               if not arg_dct.has_key(a[0]):
                  tmp_lst.extend([(None,a[0]),(None,a[1])])
               else:
                  tmp_lst.append(a)
            else:
               tmp_lst.append(a)
         lst = tmp_lst
      # make sure we don't have too many arguments
      if len(lst)>narg:
         if not narg:
            print "Error: too many arguments for %s. None expected."%(name)
         elif narg==nreq:
            print "Error: too many arguments for %s. %d expected."%(name,nreq)
            dump_arg(name,arg_nam,nreq)
         else:
            print "Error: too many arguments for %s. %d to %d expected."%(name,nreq,narg)
            dump_arg(name,arg_nam,nreq)            
         raise QuietException
      # match names to unnamed arguments to create argument dictionary
      ac = 0
      val_dct = {}
      for a in lst:
         if a[0]==None:
            if ac>=narg:
               print "Parsing-Error: ambiguous argument: '"+str(a[1])+"'"
               raise QuietException
            else:
               val_dct[arg_nam[ac]]=a[1]
         else:
            val_dct[a[0]]=a[1]
         ac = ac + 1
      # now check to make sure we don't have any missing arguments
      for a in arg_nam:
         if arg_dct[a]:
            if not val_dct.has_key(a):
               print "Parsing-Error: missing required argument:",a
               raise QuietException
      # return all arguments as keyword arguments
      kw = val_dct
# time for a little testing
   return (arg,kw)


# launching routines

def run_as_module(file,spawn=0):
   name = re.sub('[^A-Za-z0-9]','_',file)
   mod = new.module(name)
   mod.__file__ = file
   sys.modules[name]=mod
   if spawn:
      t = threading.Thread(target=execfile,
         args=(file,mod.__dict__,mod.__dict__))
      t.setDaemon(1)
      t.start()
   else:
      execfile(file,mod.__dict__,mod.__dict__)
      del sys.modules[name]
      del mod

def run_as_thread(args,global_ns,local_ns):
   t = threading.Thread(target=execfile,args=(args,global_ns,local_ns))
   t.setDaemon(1)
   t.start()
               
def split(*arg): # custom split-and-trim
   '''
split(string,token[,count]) -> list of strings
 
UTILITY FUNCTION, NOT PART OF THE API
Breaks strings up by tokens but preserves quoted strings and
parenthetical groups (such as atom selections).

USAGE OF THIS FUNCTION IS DISCOURAGED - THE GOAL IS TO
MAKE IT UNNECESSARY BY IMPROVING THE BUILT-IN PARSER
'''
   str = arg[0]
   tok = arg[1]
   if len(arg)>2:
      mx=arg[2]
   else:
      mx=0
   pair = { '(':')','[':']','{':'}',"'":"'",'"':'"' }
   plst = pair.keys()
   stack = []
   lst = []
   c = 0
   nf = 0
   l = len(str)
   wd = ""
   while str[c]==tok:
      c = c + 1
   while c<l:
      ch = str[c]
      if (ch in tok) and (len(stack)==0):
         lst.append(string.strip(wd))
         nf = nf + 1
         if mx:
            if nf==mx:
               wd = string.strip(str[c+1:])
               break;
         wd = ''
         w = 0
      else:
         if len(stack):
            if ch==stack[0]:
               stack = stack[1:]
            elif (ch in plst):
               stack[:0]=[pair[ch]]
         elif (ch in plst):
            stack[:0]=[pair[ch]]
         wd = wd + ch
      c = c + 1
   if len(wd):
      lst.append(string.strip(wd))
   return lst
   

if __name__=='__main__':

# regular expressions

   mo = arg_name_re.match("earth=testing,hello")
   tv = mo.group(0)
   print tv == "earth=",tv
   
   mo = arg_value_re.match("testing,hello")
   tv = mo.group(0)
   print tv == "testing",tv

   mo = arg_value_re.match("'testing,\"hello'")
   tv = mo.group(0)
   print tv == "'testing,\"hello'",tv

   mo = arg_value_re.match('"testing,\'hello"')
   tv = mo.group(0)
   print tv == '"testing,\'hello"',tv

   mo = arg_value_re.match("\'\'\'testing,h\"ello\'\'\'")
   tv = mo.group(0)
   print tv=="'''testing,h\"ello'''",tv



# argument parsing

   tv = parse_arg("command val")
   print tv==[(None, 'val')],tv
   tv = parse_arg("command val1,val2")
   print tv==[(None, 'val1'), (None, 'val2')],tv
   tv=parse_arg("command val1,val2,val3")
   print tv== [(None, 'val1'), (None, 'val2'), (None, 'val3')],tv      

   tv = parse_arg("command arg=val")
   print tv == [('arg', 'val')],tv
   tv = parse_arg("command arg1=val1,arg2=val2")
   print tv == [('arg1', 'val1'), ('arg2', 'val2')],tv
   tv = parse_arg("command val1,val2,val3")
   print tv == [(None, 'val1'), (None, 'val2'), (None, 'val3')],tv

   tv = parse_arg("command arg=val")
   print tv == [('arg', 'val')] ,tv
   tv = parse_arg("command arg_1=val1,arg2=val2")
   print tv == [('arg_1', 'val1'), ('arg2', 'val2')],tv
   tv = parse_arg("command val1,val2,val3")
   print tv == [(None, 'val1'), (None, 'val2'), (None, 'val3')],tv
   
   tv = parse_arg("command val1, str2=val2, str3=val3")
   print tv == [(None, 'val1'), ('str2', 'val2'), ('str3', 'val3')],tv
   tv = parse_arg("command val1, str2 = val2,str3= val3")
   print tv == [(None, 'val1'), ('str2', 'val2'), ('str3', 'val3')],tv
   tv = parse_arg("command val1, str2 =val2 ,str3 = val3")
   print tv == [(None, 'val1'), ('str2', 'val2'), ('str3', 'val3')],tv
   tv = parse_arg("command val1, str2 =val2 , str3= val3   ")
   print tv == [(None, 'val1'), ('str2', 'val2'), ('str3', 'val3')],tv
   
   tv = parse_arg("command multi word 1, str2 =multi word 2")
   print tv == [(None, 'multi word 1'), ('str2', 'multi word 2')],tv

   tv = parse_arg("command multi word 1  , str2 =  multi word 2 ")
   print tv == [(None, 'multi word 1'), ('str2', 'multi word 2')],tv

   tv = parse_arg("command ( name;ca,c,n ), sel1= (name c,n) ")
   print tv == [(None, '( name;ca,c,n )'), ('sel1', '(name c,n)')],tv

   tv = parse_arg("command ( byres (name;ca,c,n) ), sel1= (name c,n) ")
   print tv==[(None, '( byres (name;ca,c,n) )'), ('sel1', '(name c,n)')],tv

   tv = parse_arg("command ( byres (name;ca,c,n ), sel1= (name c,n)) ")
   print tv==[(None, '( byres (name;ca,c,n ), sel1= (name c,n))')],tv

   tv = parse_arg("command test,")
   print tv==[(None,'test')],tv

   tv = parse_arg("command =sdf,") # desired behavior?
   print tv==[(None,'=sdf')],tv

   tv = parse_arg("command 'hello\",bob','''hel\"lo,dad'''") 
   print tv==[(None, '\'hello",bob\''), (None, '\'\'\'hel"lo,dad\'\'\'')],tv

   tv = parse_arg("command \"'hello''bob'\"")
   print tv==[(None, '"\'hello\'\'bob\'"')],tv


   tv = parse_arg("command this,is,a command;b command",mode=LITERAL)
   print tv==[(None, 'this,is,a command;b command')], tv

   tv = parse_arg("command this,a command;b command",mode=LITERAL1)
   print tv==[(None, 'this'), (None, 'a command;b command')], tv

   tv = parse_arg("command this,is,a command;b command",mode=LITERAL2)
   print tv==[(None, 'this'), (None, 'is'), (None, 'a command;b command')],tv

   tv = parse_arg("command this,is,a=hello;b command",mode=LITERAL2)
   print tv==[(None, 'this'), (None, 'is'), (None, 'a=hello;b command')],tv
   
# expected exceptions

   try:
      tv = parse_arg("command ( byres (name;ca,c,n ), sel1= (name c,n) ")      
      print 0, "exception missed"
   except QuietException:
      print 1, "exception raised"

   try:
      tv = parse_arg("command ,")
      print 0, "exception missed"
   except QuietException:
      print 1, "exception raised"

   try:
      tv = parse_arg("command berf=,")
      print 0, "exception missed"
   except QuietException:
      print 1, "exception raised"

   try:
      tv = parse_arg("command 'hello''bob'")
      print 0, "exception missed"
   except QuietException:
      print 1, "exception raised"

# function call preparation

   def fn1(req1,req2,opt1=1,opt2=2):
      pass

   tv = prepare_call(fn1,parse_arg("dummy hello,world"))
   print tv==([], {'req2': 'world', 'req1': 'hello'}),tv

   tv = prepare_call(fn1,parse_arg("dummy hello,world,opt2=hi"))
   print tv==([], {'opt2': 'hi', 'req2': 'world', 'req1': 'hello'}),tv

   tv = prepare_call(fn1,parse_arg("dummy req1=hello,req2=world,opt2=hi")) 
   print tv==([], {'opt2': 'hi', 'req2': 'world', 'req1': 'hello'}),tv

   tv = prepare_call(fn1,parse_arg("dummy req2=world,req1=hello,opt2=hi")) 
   print tv==([], {'opt2': 'hi', 'req2': 'world', 'req1': 'hello'}),tv

   tv = prepare_call(fn1,parse_arg("dummy hello,world,give,tea"))
   print tv==([], {'opt2': 'tea', 'req2': 'world', 'req1': 'hello', 'opt1': 'give'}),tv

   tv = prepare_call(fn1,parse_arg("dummy hello,world,give,tea"),10)
   print tv==(['hello', 'world', 'give', 'tea'], {}),tv

   tv = prepare_call(fn1,parse_arg("dummy hello=world,give,tea"),12) # test legacy support
   print tv==([], {'opt2': 'tea', 'req2': 'world', 'req1': 'hello', 'opt1': 'give'}),tv

   def fn2(*arg):
      pass

   tv = prepare_call(fn2,parse_arg("dummy req1=hello,req2=world,opt2=hi")) 
   print tv==([], {'opt2': 'hi', 'req2': 'world', 'req1': 'hello'}),tv

   def fn3(*arg,**kw):
      pass

   tv = prepare_call(fn3,parse_arg("dummy req1=hello,req2=world,opt2=hi")) 
   print tv==([], {'opt2': 'hi', 'req2': 'world', 'req1': 'hello'}),tv

   def fn4(req1,req2,**kw):
      pass

   tv = prepare_call(fn4,parse_arg("dummy req1=hello,req2=world,opt2=hi")) 
   print tv==([], {'opt2': 'hi', 'req2': 'world', 'req1': 'hello'}),tv


# PyMOL

from pymol import cmd
from chempy.champ import Champ

import string


print '''# NOTE: This file was autogenerated
aa_dict = {'
'''

for res in (  'ala','arg','asp','asn','cys','gln','glu','gly','his','ile','leu','lys','met','phe','pro','ser','thr','trp','tyr','val' ):

   print "'"+string.upper(res)+"': ["
   
   ch = Champ()
   cmd.fragment(res,"tmp")
   model = cmd.get_model("tmp")
   cmd.delete("tmp")
   
   m1 = ch.insert_model(model)
   ch.pattern_detect_chirality(m1)
   ch.pattern_orient_bonds(m1)
   pat1 = ch.pattern_get_string_with_names(m1)
   ch.pattern_detect_chirality(m1)
   pat2 = ch.pattern_get_string_with_names(m1)

# confirm that CHAMP handles this pattern well...

   if pat1!=pat2:
      print " Chirality Assignment Error!"
      print pat1
      print pat2
      break
   else:
      pat = pat1

#   print pat1
   clean_pat = re.sub("<[^>]*>","",pat)
   source = ch.insert_pattern_string(clean_pat)

#   print ch.pattern_get_string(source)
#   print ch.pattern_get_string(m1)
   if ch.match_1v1_b(source,m1)==0:
      print " Error: Pattern doesn't match with self!"

   pat_isomer = string.replace(pat,"@@","X")
   pat_isomer = string.replace(pat_isomer,"@","@@")
   pat_isomer = string.replace(pat_isomer,"X","@")
   isomer = ch.insert_pattern_string(pat_isomer)
   if ch.match_1v1_b(isomer,m1)==1:
      if(res!='gly'):
         print " Error: Pattern matches the enatiomer"

   pat_racemic = string.replace(pat,"@","")
   racemic = ch.insert_pattern_string(pat_racemic)
   
#   print ch.pattern_get_string(source)
#   print ch.pattern_get_string(isomer)
#   print ch.pattern_get_string(racemic)

   if ch.match_1v1_b(racemic,source)!=1:
      print " Error: Racemic doesn't match source isomer"
   
   if ch.match_1v1_b(racemic,isomer)!=1:
      print " Error: Racemic doesn't match enantiomer"
   
   if ch.match_1v1_b(source,racemic)!=0:
      print " Error: Source matches racemic"

   if ch.match_1v1_b(isomer,racemic)!=0:
      print " Error: Isomer matches racemic"
   
   lst = []
   for a in string.split(pat,'<'):
      for b in string.split(a,'>'):
         lst.append(b)
   
   tag_count = 0
   pat_list = []
   tag_list = []
   flag = 0
   while 1:
      if lst==[]:
         break
      pat_list.append(lst.pop(0))
      if lst==[]:
         break;
      tag_list.append((tag_count,lst.pop(0)))
      pat_list.append("<%d>"%tag_count)
      tag_count = tag_count+1

   new_pat = string.join(pat_list,'')
   print "   '"+new_pat+"',"

   
   print "   ["
   for a in tag_list:
      print "      (%2d, '%s'),"%a
   print "   ]"
   print "],"

print "}"

   
   

   

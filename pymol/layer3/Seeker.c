/* 
A* -------------------------------------------------------------------
B* This file contains source code for the PyMOL computer program
C* copyright 1998-2000 by Warren Lyford Delano of DeLano Scientific. 
D* -------------------------------------------------------------------
E* It is unlawful to modify or remove this copyright notice.
F* -------------------------------------------------------------------
G* Please see the accompanying LICENSE file for further information. 
H* -------------------------------------------------------------------
I* Additional authors of this source file include:
-* 
-* 
-*
Z* -------------------------------------------------------------------
*/

#include"os_predef.h"
#include"os_std.h"
#include"os_gl.h"
#include"Err.h"
#include"Util.h"
#include"Seq.h"
#include"Seeker.h"
#include"MemoryDebug.h"
#include"Executive.h"
#include"P.h"
#include"Selector.h"
#include"Wizard.h"
#include"Scene.h"
#include"Menu.h"

#define cTempSeekerSele "_seeker"
#define cTempCenterSele "_seeker_center"
#define cTempSeekerSele2 "_seeker2"
typedef struct {
  CSeqHandler handler; /* must be first */
  int drag_start_col, drag_last_col;
  int drag_row;
  int drag_dir,drag_start_toggle;
  int dragging, drag_setting;
  int drag_button;
  double LastClickTime;
} CSeeker;

CSeeker Seeker;
static CSeqRow* SeekerDrag(CSeqRow* rowVLA,int row,int col,int mod);


static void BuildSeleFromAtomList(char *obj_name,int *atom_list,char *sele_name,int start_fresh)
{
  ObjectMolecule *obj = ExecutiveFindObjectMoleculeByName(obj_name);

  if(start_fresh) {
    SelectorCreateOrderedFromObjectIndices(sele_name, obj, atom_list,-1);
  } else {
    OrthoLineType buf1;

    SelectorCreateOrderedFromObjectIndices(cTempSeekerSele2, obj,atom_list,-1);    

    sprintf(buf1,"?%s|?%s",sele_name,cTempSeekerSele2);
    SelectorCreate(sele_name,buf1,NULL,true,NULL);      
    ExecutiveDelete(cTempSeekerSele2);    
  }
}

static void SeekerSelectionToggleRange(CSeqRow* rowVLA,int row_num,
                                  int col_first,int col_last,int inc_or_excl,
                                  int start_over)
{
  char selName[ObjNameMax];
  OrthoLineType buf1,buf2;

  if(row_num>=0) {
    CSeqRow *row;
    CSeqCol *col;
    char prefix[3]="";
    int logging = SettingGet(cSetting_logging);
    int col_num;
    register int *atom_vla = NULL;
    register int n_at = 0;
    register int at_idx;
    register int *atom_list;

    ObjectMolecule *obj;
    if(logging==cPLog_pml)
      strcpy(prefix,"_ ");
    row = rowVLA + row_num;
    if( (obj = ExecutiveFindObjectMoleculeByName(row->name)) ) {
      atom_vla = VLAlloc(int,obj->NAtom/10);
      for(col_num=col_first;col_num<=col_last;col_num++) {
        col = row->col + col_num;
        if(!col->spacer) {
          if(!start_over) {
            if(inc_or_excl)
              col->inverse = true;
            else
              col->inverse = false;
          } else {
            col->inverse = true;
          }
          atom_list = row->atom_lists + col->atom_at;
          while((at_idx=(*(atom_list++)))>=0) { /* build one extra long list 
                                    so that we only call selector once*/
            VLACheck(atom_vla,int,n_at);
            atom_vla[n_at++] = at_idx;
          }
        }
      }
      VLACheck(atom_vla,int,n_at);
      atom_vla[n_at]=-1;
      BuildSeleFromAtomList(row->name,atom_vla,cTempSeekerSele,true);
      VLAFreeP(atom_vla);
      
      {      
        char *sele_mode_kw;
        sele_mode_kw = SceneGetSeleModeKeyword();
        
        if(logging) SelectorLogSele(cTempSeekerSele);
        
        if(!WizardDoSelect(cTempSeekerSele)) {
          
          ExecutiveGetActiveSeleName(selName,true);
          
          /* selection or deselecting? */
          
          if(!start_over) {
            if(inc_or_excl) {
              sprintf(buf1,"((%s(?%s)) or %s(?%s))",
                      sele_mode_kw,selName,sele_mode_kw,cTempSeekerSele);
            } else {
              sprintf(buf1,"((%s(?%s)) and not %s(?%s))",
                      sele_mode_kw,selName,sele_mode_kw,cTempSeekerSele);
            }
          } else {
            sprintf(buf1,"%s(?%s)",sele_mode_kw,cTempSeekerSele);
          }
          
          /* create the new active selection */
          
          SelectorCreate(selName,buf1,NULL,true,NULL);
          {
            sprintf(buf2,"%scmd.select(\"%s\",\"%s\")\n",prefix,selName,buf1);
            PLog(buf2,cPLog_no_flush);
          }
        }
        
        ExecutiveDelete(cTempSeekerSele);
        if(logging) {
          sprintf(buf2,"%scmd.delete(\"%s\")\n",prefix,cTempSeekerSele);
          PLog(buf2,cPLog_no_flush);
          PLogFlush();
        }
        
        if(SettingGet(cSetting_auto_show_selections))
          ExecutiveSetObjVisib(selName,1);
        SceneDirty();
      }
    }
  }
}

static void SeekerSelectionToggle(CSeqRow* rowVLA,int row_num,
                                  int col_num,int inc_or_excl,
                                  int start_over)
{
  char selName[ObjNameMax];
  OrthoLineType buf1,buf2;

  if(row_num>=0) {
    CSeqRow *row;
    CSeqCol *col;
    int *atom_list;
    char prefix[3]="";
    int logging = SettingGet(cSetting_logging);

    if(logging==cPLog_pml)
      strcpy(prefix,"_ ");
    row = rowVLA + row_num;
    col = row->col + col_num;
    if(!col->spacer) 
      if( ExecutiveFindObjectByName(row->name)) {
        char *sele_mode_kw;
        atom_list = row->atom_lists + col->atom_at;
        
        /* build up a selection consisting of residue atoms */
        
        BuildSeleFromAtomList(row->name,atom_list,cTempSeekerSele,true);
        sele_mode_kw = SceneGetSeleModeKeyword();

        if(logging) SelectorLogSele(cTempSeekerSele);
        
        if(!WizardDoSelect(cTempSeekerSele)) {
          
          ExecutiveGetActiveSeleName(selName,true);
          
          /* selection or deselecting? */

          if(!start_over) {
            if(inc_or_excl) {
              if(!col->spacer) {
                col->inverse = true;
                sprintf(buf1,"((%s(?%s)) or %s(%s))",
                        sele_mode_kw,selName,sele_mode_kw,cTempSeekerSele);
              }
            } else {
              if(!col->spacer) {
                col->inverse = false;
                sprintf(buf1,"((%s(?%s)) and not %s(%s))",
                        sele_mode_kw,selName,sele_mode_kw,cTempSeekerSele);
              }
            }
          } else {
            if(!col->spacer) {
              col->inverse = true;
              sprintf(buf1,"%s(%s)",sele_mode_kw,cTempSeekerSele);
            }
          }
          
          /* create the new active selection */
          
          SelectorCreate(selName,buf1,NULL,true,NULL);
          {
            sprintf(buf2,"%scmd.select(\"%s\",\"%s\")\n",prefix,selName,buf1);
            PLog(buf2,cPLog_no_flush);
          }
        }

        ExecutiveDelete(cTempSeekerSele);
        if(logging) {
          sprintf(buf2,"%scmd.delete(\"%s\")\n",prefix,cTempSeekerSele);
          PLog(buf2,cPLog_no_flush);
          PLogFlush();
        }
        
        if(SettingGet(cSetting_auto_show_selections))
          ExecutiveSetObjVisib(selName,1);
        SceneDirty();
      }
  }
}


static void SeekerSelectionUpdateCenter(CSeqRow* rowVLA,int row_num,int col_num,int start_over)
{
  
  {
    CSeqRow *row;
    CSeqCol *col;
    CObject *obj;

    int *atom_list;
    char prefix[3]="";
    int logging = SettingGet(cSetting_logging);


    if(logging==cPLog_pml)
      strcpy(prefix,"_ ");
    if(row_num>=0) {
      row = rowVLA + row_num;
      col = row->col + col_num;
      
      if(!col->spacer)
        if( (obj = ExecutiveFindObjectByName(row->name))){
          
          if(col->state&& obj )
            SettingSetSmart_i(obj->Setting,NULL,cSetting_state,col->state);
          
          atom_list = row->atom_lists + col->atom_at;
          
          BuildSeleFromAtomList(row->name,atom_list,cTempCenterSele,start_over);
          if(logging) SelectorLogSele(cTempCenterSele);
        }
    }
  }

}

static void SeekerSelectionCenter(int action)
{
  OrthoLineType buf2;
  char prefix[3]="";
  int logging = SettingGet(cSetting_logging);
  if(logging==cPLog_pml)
    strcpy(prefix,"_ ");
  
  switch(action) {
  case 0: /* center cumulative*/
    ExecutiveCenter(cTempCenterSele,-1,true);
    if(logging) {
      sprintf(buf2,"%scmd.center(\"%s\")\n",prefix,cTempCenterSele);
      PLog(buf2,cPLog_no_flush);
      PLogFlush();
    }
    break;
  case 1: /* zoom */
    ExecutiveWindowZoom(cTempCenterSele,0.0,-1,false);
    if(logging) {
      sprintf(buf2,"%scmd.zoom(\"%s\")\n",prefix,cTempCenterSele);
      PLog(buf2,cPLog_no_flush);
      PLogFlush();
    }
    break;
  case 2: /* center seeker */
    {
      char selName[ObjNameMax];
      if(ExecutiveGetActiveSeleName(selName,true)) {
        ExecutiveCenter(selName,-1,true);
        if(logging) {
          sprintf(buf2,"%scmd.center(\"%s\")\n",prefix,selName);
          PLog(buf2,cPLog_no_flush);
          PLogFlush();
        }
      }
    }
    break;
  }
}

#define cDoubleTime 0.35

static CSeqRow* SeekerClick(CSeqRow* rowVLA,int button,int row_num,int col_num,int mod,int x,int y)
{
  CSeqRow *row;
  CSeqCol *col;
  /*  char selName[ObjNameMax]; */
  CSeeker *I = &Seeker;    
  int continuation = false;
  if((row_num<0)||(col_num<0)) {
    switch(button) {
    case P_GLUT_LEFT_BUTTON:
      if((UtilGetSeconds()-I->LastClickTime)<cDoubleTime) {
        OrthoLineType buf2;
        char name[ObjNameMax];
        if(ExecutiveGetActiveSeleName(name, false)) {
          SelectorCreate(name,"none",NULL,true,NULL);
          if(SettingGet(cSetting_logging)) {
            sprintf(buf2,"cmd.select('%s','none')\n",name);
            PLog(buf2,cPLog_no_flush);
          }
          SeqDirty();
          
        }
      }
      I->LastClickTime = UtilGetSeconds();
      break;
    }
  } else {
    row = rowVLA + row_num;
    col = row->col + col_num;
    I->dragging = false;
    I->drag_button = button;
    I->handler.box_row = row_num;
    I->handler.box_stop_col = col_num;
    if((I->drag_row==row_num)&&
       (button==P_GLUT_LEFT_BUTTON) &&
       (mod & cOrthoSHIFT)) {
      continuation = true;
    } else {
      I->drag_row = -1; /* invalidate */
      I->handler.box_start_col = col_num;
    }
    
    switch(button) {
    case P_GLUT_RIGHT_BUTTON:
      {
        ObjectMolecule *obj;
        char name[ObjNameMax];

        if(ExecutiveGetActiveSeleName(name, false) && col->inverse) {
          MenuActivate2Arg(x,y+16,x,y,"pick_option",name,name);
        } else if( (obj = ExecutiveFindObjectMoleculeByName(row->name) )) {
          OrthoLineType buffer;
          {
            int *atom_list;
            char prefix[3]="";
            int logging = SettingGet(cSetting_logging);
            
            if(logging==cPLog_pml)
              strcpy(prefix,"_ ");
            
            if( ExecutiveFindObjectByName(row->name)) {
              atom_list = row->atom_lists + col->atom_at;
              
              /* build up a selection consisting of residue atoms */
              
              if((*atom_list)>=0) {
                
                ObjectMoleculeGetAtomSele(obj,*atom_list,buffer);
                
                BuildSeleFromAtomList(row->name,atom_list,cTempSeekerSele,true);
                if(logging) SelectorLogSele(cTempSeekerSele);
                
                MenuActivate2Arg(x,y+16,x,y,"seq_option",buffer,cTempSeekerSele); 
                
              }
            }
          }
        }
      }
      break;
    case P_GLUT_MIDDLE_BUTTON:
      if(!col->spacer) {
        ObjectMolecule *obj;
        I->drag_start_col = col_num;
        I->drag_last_col = col_num;
        I->drag_row = row_num;
        I->dragging = true;
        SeekerSelectionUpdateCenter(rowVLA,row_num,col_num,true);
        if(mod & cOrthoCTRL) 
          SeekerSelectionCenter(1);
        else
          SeekerSelectionCenter(0);
        I->handler.box_active=true;
        if(col->state && (obj = ExecutiveFindObjectMoleculeByName(row->name) )) {
          SettingSetSmart_i(obj->Obj.Setting,NULL,cSetting_state,col->state);
          SceneChanged();
        }
      }
      break;
    case P_GLUT_LEFT_BUTTON:
      if(!col->spacer) {
        int start_over=false;
        int center = 0;
        ObjectMolecule *obj;
        if(mod & cOrthoCTRL) {
          center = 2;
        }
        if(!continuation) {
          I->drag_start_col = col_num;
          I->drag_last_col = col_num;
          I->drag_row = row_num;
          I->drag_dir = 0;
          I->drag_start_toggle = true;
        } else {
          int tmp;
          if(((col_num<I->drag_start_col)&&(I->drag_last_col>I->drag_start_col)) ||
             ((col_num>I->drag_start_col)&&(I->drag_last_col<I->drag_start_col))) {
              tmp = I->drag_last_col;
              I->drag_last_col=I->drag_start_col;
              I->drag_start_col = tmp;
              I->drag_dir = -I->drag_dir;
          }
        }
        I->dragging = true;


        I->handler.box_active=true;
        if(continuation) {
          SeekerDrag(rowVLA,row_num,col_num,mod);
        } else {
          if(col->inverse&&!start_over) {
            SeekerSelectionToggle(rowVLA,row_num,col_num,false,false);
            I->drag_setting = false;
          } else {
            SeekerSelectionToggle(rowVLA,row_num,col_num,true,start_over);
            I->drag_setting = true;
          }
        }
        if(center)
          SeekerSelectionCenter(2);

        if(col->state && (obj = ExecutiveFindObjectMoleculeByName(row->name))) {
          SettingSetSmart_i(obj->Obj.Setting,NULL,cSetting_state,col->state);
          SceneChanged();
        }
      }
      break;
    }
  }

  return NULL;
}

static void SeekerRefresh(CSeqRow *rowVLA)
{
  if(rowVLA) {
    CSeqRow *row;
    CSeqCol *col;
    int *atom_list;
    int nRow = VLAGetSize(rowVLA);
    int sele = ExecutiveGetActiveSele();
    int b;
    ObjectMolecule *obj;

    for(b=0;b<nRow;b++) {
      row = rowVLA + b;
      
      if( (obj = ExecutiveFindObjectMoleculeByName(row->name)) ) {
        register int a;
        register AtomInfoType *atInfo = obj->AtomInfo;
        register int at;
        register int selected;
        register int not_selected;
        
        if(sele<0) {
          for(a=0;a<row->nCol;a++) {
            col = row->col + a;
            col->inverse = false;
          }
        } else {
          for(a=0;a<row->nCol;a++) {
            
            col = row->col + a;
            if(!col->spacer) {
              selected = false;
              atom_list = row->atom_lists + col->atom_at;
              not_selected = true;
              
              while( (at=(*atom_list)) >=0) {
                atom_list++;
                if(SelectorIsMember(atInfo[at].selEntry,sele)) {
                  selected = true; 
                } else {
                  not_selected = true;
                }
              }
              
              if(selected)
                col->inverse = true;
              else
                col->inverse = false;
            } else 
              col->inverse = false;
          }
        }
      }
    }
  }
}

static CSeqRow* SeekerDrag(CSeqRow* rowVLA,int row,int col,int mod)
{
  CSeeker *I = &Seeker;    
  int a;

  if((row>=0)&&(col>=0)&&(I->dragging)) {
    I->handler.box_stop_col = col;
    
    switch(I->drag_button) {
    case P_GLUT_LEFT_BUTTON:
      if(col != I->drag_last_col) {

        if(I->drag_dir) {
          if(I->drag_dir>0) {
            if(col<=I->drag_start_col) {
              col = I->drag_start_col;
              if(I->drag_start_toggle) {
                SeekerSelectionToggle(rowVLA,I->drag_row,I->drag_start_col,!I->drag_setting,false);  
                I->drag_start_toggle = false;
              }
            } else if(col>I->drag_start_col) {
              if(!I->drag_start_toggle) {
                SeekerSelectionToggle(rowVLA,I->drag_row,I->drag_start_col,I->drag_setting,false);  
                I->drag_start_toggle = true;
              }
            }
          } else if(I->drag_dir<0) {
            if(col>=I->drag_start_col) {
              col = I->drag_start_col;
              if(I->drag_start_toggle) {
                SeekerSelectionToggle(rowVLA,I->drag_row,I->drag_start_col,!I->drag_setting,false);  
                I->drag_start_toggle = false;
              }
            } else if (col<I->drag_start_col) {
              if(!I->drag_start_toggle) {
                SeekerSelectionToggle(rowVLA,I->drag_row,I->drag_start_col,I->drag_setting,false);  
                I->drag_start_toggle = true;
              }
            }
          }
        }
        /*
        if(mod &cOrthoSHIFT) {
          if(I->drag_start_col == I->drag_last_col) {
            if(col>I->drag_start_col) {
              SeekerSelectionCenter(rowVLA,I->drag_row,I->drag_start_col+1,false);
            } else if(col<I->drag_start_col) {
              SeekerSelectionCenter(rowVLA,I->drag_row,I->drag_start_col-1,false);
            }
          }
          if(I->drag_start_col < I->drag_last_col) {
            if( col > I->drag_last_col ) {
              for( a=I->drag_last_col+1; a<=col; a++) {
                SeekerSelectionCenter(rowVLA,I->drag_row,a,false);
              }
            }
          } else {
            
            if( col < I->drag_last_col) {
              for(a=I->drag_last_col-1;a>=col;a--) {
                SeekerSelectionCenter(rowVLA,I->drag_row,a,false);
              }
            }
          }
          SeekerSelectionCenter(0);
        }
        */

        if((I->drag_last_col<I->drag_start_col) && (col>I->drag_start_col))
          {
            /*            for(a=I->drag_last_col;a<I->drag_start_col;a++)*/
            SeekerSelectionToggleRange(rowVLA,I->drag_row,I->drag_last_col,I->drag_start_col-1,!I->drag_setting,false);  
            I->drag_last_col = I->drag_start_col;
          }
        if((I->drag_last_col>I->drag_start_col) && (col<I->drag_start_col))
          {
            /*            for(a=I->drag_last_col;a>I->drag_start_col;a--)*/
            SeekerSelectionToggleRange(rowVLA,I->drag_row,I->drag_start_col+1,I->drag_last_col,!I->drag_setting,false);
            I->drag_last_col = I->drag_start_col;
          }
        if(I->drag_start_col == I->drag_last_col) {
          if(col>I->drag_start_col) {
            if(!I->drag_dir)
              I->drag_dir = 1;
            I->drag_last_col = I->drag_start_col+1;
            SeekerSelectionToggle(rowVLA,I->drag_row,I->drag_last_col,I->drag_setting,false);
          } else if(col<I->drag_start_col){
            if(!I->drag_dir)
              I->drag_dir = -1;
            I->drag_last_col = I->drag_start_col-1;          
            SeekerSelectionToggle(rowVLA,I->drag_row,I->drag_last_col,I->drag_setting,false);
          }
        }
        if(I->drag_start_col < I->drag_last_col) {
          
          if( col > I->drag_last_col ) {
            /*            for( a=I->drag_last_col+1; a<=col; a++) */
            SeekerSelectionToggleRange(rowVLA,I->drag_row,I->drag_last_col+1,col,I->drag_setting,false);          
          } else {
            /*            for(a=I->drag_last_col; a>col ;a--) */
            SeekerSelectionToggleRange(rowVLA,I->drag_row,col+1,I->drag_last_col,!I->drag_setting,false);          
          }
        } else {
          
          if( col < I->drag_last_col) {
            /*for(a=I->drag_last_col-1;a>=col;a--) */
            SeekerSelectionToggleRange(rowVLA,I->drag_row,col,I->drag_last_col-1,I->drag_setting,false);          
          } else {
            /*for(a=I->drag_last_col; a<col ;a++) */
            SeekerSelectionToggleRange(rowVLA,I->drag_row,I->drag_last_col,col-1,!I->drag_setting,false);          
          }
        }
        I->drag_last_col = col;              
       
        if(mod & cOrthoCTRL) {
          SeekerSelectionCenter(2);
        }
 
      }
      break;
    case P_GLUT_MIDDLE_BUTTON:
      if(col != I->drag_last_col) {
        int action=0;
        int start_over = false;

        if(mod & cOrthoCTRL) {
          action = 1;
        }
        if(!(mod & cOrthoSHIFT)) {
          start_over = true;
          I->handler.box_start_col = col;
          SeekerSelectionUpdateCenter(rowVLA,I->drag_row,col,start_over);          
        } else {
          if(I->drag_start_col == I->drag_last_col) {
            if(col>I->drag_start_col) {
              I->drag_last_col = I->drag_start_col+1;
              SeekerSelectionUpdateCenter(rowVLA,I->drag_row,I->drag_last_col,start_over);          
            } else if(col<I->drag_start_col) {
              I->drag_last_col = I->drag_start_col-1;          
              SeekerSelectionUpdateCenter(rowVLA,I->drag_row,I->drag_last_col,start_over);          
            }
          }
          if(I->drag_start_col < I->drag_last_col) {
            
            if( col > I->drag_last_col ) {
              for( a=I->drag_last_col+1; a<=col; a++) {
                SeekerSelectionUpdateCenter(rowVLA,I->drag_row,a,start_over);          
              }
            }
          } else {
            
            if( col < I->drag_last_col) {
              for(a=I->drag_last_col-1;a>=col;a--) {
                SeekerSelectionUpdateCenter(rowVLA,I->drag_row,a,start_over);          
              }
            }
          }
        }
        I->drag_last_col = col;              
        
        SeekerSelectionCenter(action);
      }
      break;
    }
  }
  return NULL;
}
  
static CSeqRow* SeekerRelease(CSeqRow* rowVLA,int button,
                              int row,int col,int mod)
{
  CSeeker *I = &Seeker;    
  I->dragging = false;

  I->handler.box_active=false;
  return NULL;
}


static char SeekerGetAbbr(char *abbr)
{
  
  switch(abbr[0]) {
  case 'A':
    switch(abbr[1]) {
    case 'L': 
      if(abbr[2]=='A')
        return 'A';
      break;
    case 'R': 
      if(abbr[2]=='G')
        return 'R';
      break;
    case 'S': 
      switch(abbr[2]) {
      case 'P':
        return 'D';
        break;
      case 'N':
        return 'N';
        break;
      }
      break;
    }
    break;
  case 'C':
    switch(abbr[1]) {
    case 'Y': 
      switch(abbr[2]) {
      case 'S':
      case 'X':
        return 'C';
        break;
      }
      break;
    }
    break;
  case 'G':
    switch(abbr[1]) {
    case 'L': 
      switch(abbr[2]) {
      case 'N':
        return 'Q';
        break;
      case 'U':
        return 'E';
        break;
      case 'Y':
        return 'G';
        break;
      }
    }
    break;
  case 'H':
    switch(abbr[1]) {
    case 'I': 
      switch(abbr[2]) {
      case 'S':
      case 'D':
      case 'E':
        return 'H';
        break;
      }
      break;
    case 'O': 
      switch(abbr[2]) {
      case 'H':
        return 'O';
        break;
      }
      break;
    case '2': 
      switch(abbr[2]) {
      case 'O':
        return 'O';
        break;
      }
      break;
    }
  case 'I':
    switch(abbr[1]) {
    case 'L': 
      switch(abbr[2]) {
      case 'E':
        return 'I';
        break;
      }
    }
    break;
  case 'L':
    switch(abbr[1]) {
    case 'E': 
      switch(abbr[2]) {
      case 'U':
        return 'L';
        break;
      }
      break;
    case 'Y': 
      switch(abbr[2]) {
      case 'S':
        return 'K';
        break;
      }
      break;
    }
    break;
  case 'M':
    switch(abbr[1]) {
    case 'E': 
      switch(abbr[2]) {
      case 'T':
        return 'M';
        break;
      }
    }
    break;
  case 'P':
    switch(abbr[1]) {
    case 'H':
      switch(abbr[2]) {
      case 'E':
        return 'F';
        break;
      }
      break;     
    case 'R': 
      switch(abbr[2]) {
      case 'O':
        return 'P';
        break;
      }
      break;
    }
    break;
  case 'S':
    switch(abbr[1]) {
    case 'E': 
      switch(abbr[2]) {
      case 'R':
        return 'S';
        break;
      }
      break;
    case 'O':  /* SOL -- gromacs solvent residue */
      switch(abbr[2]) {
      case 'L':
        return 'O';
        break;
      }
      break;
    }
    break;
  case 'T':
    switch(abbr[1]) {
    case 'H': 
      switch(abbr[2]) {
      case 'R':
        return 'T';
        break;
      }
      break;
    case 'I': 
      switch(abbr[2]) {
      case 'P':
        return 'O';
        break;
      }
      break;
    case 'R': 
      switch(abbr[2]) {
      case 'P':
        return 'W';
        break;
      }
      break;
    case 'Y': 
      switch(abbr[2]) {
      case 'R':
        return 'Y';
        break;
      }
      break;
    }
    break;
  case 'V':
    switch(abbr[1]) {
    case 'A': 
      switch(abbr[2]) {
      case 'L':
        return 'V';
        break;
      }
      break;
    }
    break;
  case 'W':
    switch(abbr[1]) {
    case 'A': 
      switch(abbr[2]) {
      case 'T':
        return 'O';
        break;
      }
      break;
    }
    break;

  }

  return 0;
}

static int FindColor(AtomInfoType *ai,int n_more)
{
  int result = ai->color; /* default -- use first atom color */
  AtomInfoType *ai0 =ai;
  while(1) {
    if(ai0->flags & cAtomFlag_guide) /* best use guide color */
      return ai0->color;
    if(ai0->protons == cAN_C) /* or use carbon color */
      result = ai0->color;
    n_more--;
    if(n_more>0) {
      ai0++;
      if(!AtomInfoSameResidueP(ai,ai0))
        break;
    } else 
      break;
  }
  return result;
}

void SeekerUpdate(void)
{
  /*  CObject *o = NULL;
      int s;*/

  void *hidden = NULL;
  AtomInfoType *ai;
  ObjectMolecule *obj;
  int nRow = 0;
  int label_mode = 0;
  int codes = 0;
  int max_row = 50;
  int default_color = 0;
  CSeqRow *row_vla,*row,*lab=NULL;
  row_vla = VLACalloc(CSeqRow,10);
  /* FIRST PASS: get all the residues represented properly */
  label_mode = SettingGetGlobal_i(cSetting_seq_view_label_mode);

  while(ExecutiveIterateObjectMolecule(&obj,&hidden)) {
    if(obj->Obj.Enabled&&(SettingGet_b(obj->Obj.Setting,NULL,cSetting_seq_view))&&
       (obj->Obj.Name[0]!='_')) {
      int a;
      AtomInfoType *last = NULL,*last_segi=NULL,*last_chain = NULL;
      CoordSet *last_disc = NULL;
      int last_state;
      int last_abbr = false;
      int nCol = 0;
      int nListEntries = 1; /* first list starts at 1 always... */
      int est_col = obj->NAtom/5+1;
      int est_char = obj->NAtom*4;
      int first_atom_in_label;
      int min_pad = -1;
      CSeqCol *r1 = NULL,*l1=NULL;/* *col */

      if(nRow>=max_row)
        break;

      codes = SettingGet_i(obj->Obj.Setting,NULL,cSetting_seq_view_format);
      default_color = SettingGet_i(obj->Obj.Setting,NULL,cSetting_seq_view_color);

      /* allocate a row for labels, if present
         the text for the labels and the residues will line up exactly 
      */

      VLACheck(row_vla,CSeqRow,nRow);
      if((label_mode==2)||((label_mode==1)&&(!nRow)))
        {
          lab = row_vla + nRow++;
          lab->txt = VLAlloc(char,est_char);
          lab->col = VLACalloc(CSeqCol,est_col);
          lab->label_flag = true;
        }
      else
        lab = NULL;

      VLACheck(row_vla,CSeqRow,nRow);

      row = row_vla+nRow;
      if(lab) lab = row-1; /* critical! */
      row->txt = VLAlloc(char,est_char);
      row->col = VLACalloc(CSeqCol,est_col);
      row->atom_lists = VLACalloc(int,obj->NAtom+est_col+1);
      row->atom_lists[0] = -1; /* terminate the blank listQ (IMPORTANT!) */
      row->char2col = VLACalloc(int,est_char);
      row->obj = obj;
      strcpy(row->name,obj->Obj.Name);
      row->color = obj->Obj.Color;
      ai = obj->AtomInfo;

      /* copy object name onto label row */

      if(lab) {
        
        int st_len;
        /* copy label text */

        VLACheck(lab->col,CSeqCol,nCol);
        l1 = lab->col + nCol;
        l1->start = lab->len;
        UtilConcatVLA(&lab->txt,&lab->len,"/");
        UtilConcatVLA(&lab->txt,&lab->len,obj->Obj.Name);
        l1->stop = lab->len;
        st_len = l1->stop - l1->start;

      } 
      if(label_mode<2) { /* no label rows, so put object name into left-hand column */

        /* copy label text */

        VLACheck(row->col,CSeqCol,nCol);
        r1 = row->col + nCol;
        r1->start = row->len;
        UtilConcatVLA(&row->txt,&row->len,"/");
        UtilConcatVLA(&row->txt,&row->len,obj->Obj.Name);
        r1->stop = row->len;
        r1->spacer = true;
        row->column_label_flag = true;
        row->title_width = row->len;
        nCol++;
      }
      if(label_mode>0) {
        /* blank equivalent text for sequence row below the fixed label */
        VLACheck(row->col,CSeqCol,nCol);
        r1 = row->col + nCol;
        r1->start = row->len;
        UtilFillVLA(&row->txt,&row->len,' ',1);
        r1->stop = row->len;
        r1->spacer = true;
        nCol++;
      }

      if(lab) {
        
        int st_len;
        /* copy label text */

        VLACheck(lab->col,CSeqCol,nCol);
        l1 = lab->col + nCol;
        l1->start = lab->len;
        UtilConcatVLA(&lab->txt,&lab->len,"/");
        UtilConcatVLA(&lab->txt,&lab->len,ai->segi);
        UtilConcatVLA(&lab->txt,&lab->len,"/");
        UtilConcatVLA(&lab->txt,&lab->len,ai->chain);
        UtilConcatVLA(&lab->txt,&lab->len,"/");
        l1->stop = lab->len;
        st_len = l1->stop - l1->start;

        last_segi = ai;
        last_chain = ai;
        /* blank equivalent text for sequence row below the fixed label */
        VLACheck(row->col,CSeqCol,nCol);
        r1 = row->col + nCol;
        r1->start = row->len;
        UtilFillVLA(&row->txt,&row->len,' ',st_len);
        r1->stop = row->len;
        r1->spacer = true;
        nCol++;
      }

      last_state=-1;
      for(a=0;a<obj->NAtom;a++) {
        first_atom_in_label = false;
        if(lab&&!AtomInfoSameSegmentP(last_segi,ai)) {

          int st_len;

          if(row->len<min_pad) {
            row->len = min_pad;
          }
          min_pad = -1;

          /* copy label text */
          
          VLACheck(lab->col,CSeqCol,nCol);
          l1 = lab->col + nCol;
          l1->start = lab->len;
          UtilConcatVLA(&lab->txt,&lab->len,"/");
          UtilConcatVLA(&lab->txt,&lab->len,ai->segi);
          UtilConcatVLA(&lab->txt,&lab->len,"/");
          UtilConcatVLA(&lab->txt,&lab->len,ai->chain);
          UtilConcatVLA(&lab->txt,&lab->len,"/");
          l1->stop = lab->len;
          st_len = l1->stop - l1->start;

          /* blank equivalent text for sequence row */
          VLACheck(row->col,CSeqCol,nCol);
          r1 = row->col + nCol;
          r1->start = row->len;
          UtilFillVLA(&row->txt,&row->len,' ',st_len);
          r1->stop = row->len;
          r1->spacer = true;
          nCol++;
          
          last_abbr = false;
          
          last_segi = ai;
          last_chain = ai;

        } else if(lab&&!AtomInfoSameChainP(last_chain,ai)) {

          int st_len;

          if(row->len<min_pad) {
            row->len = min_pad;
          }
          min_pad = -1;

          /* copy label text */
          
          VLACheck(lab->col,CSeqCol,nCol);
          l1 = lab->col + nCol;
          l1->start = lab->len;
          UtilConcatVLA(&lab->txt,&lab->len,"/");
          UtilConcatVLA(&lab->txt,&lab->len,ai->chain);
          UtilConcatVLA(&lab->txt,&lab->len,"/");
          l1->stop = lab->len;
          st_len = l1->stop - l1->start;
          
          /* blank equivalent text for sequence row */
          VLACheck(row->col,CSeqCol,nCol);
          r1 = row->col + nCol;
          r1->start = row->len;
          UtilFillVLA(&row->txt,&row->len,' ',st_len);
          r1->stop = row->len;
          r1->spacer = true;
          nCol++;
          
          last_abbr = false;
          last_chain = ai;
        }

        if(min_pad<0)
          min_pad = strlen(ai->resi) + row->len + 1;
        
        switch(codes) {
        case 0: /* one letter residue codes */
          if(!AtomInfoSameResidueP(last,ai)) {
            char abbr[2] = "1";
            last = ai;            

            VLACheck(row->col,CSeqCol,nCol);
            r1 = row->col+nCol;
            r1->start = row->len;
            if(obj->DiscreteFlag) 
              r1->state = ai->discrete_state;

            first_atom_in_label = true;

            abbr[0] = SeekerGetAbbr(ai->resn);
            
            if(!abbr[0]) {
              if(last_abbr) {
                UtilConcatVLA(&row->txt,&row->len," ");                
                r1->start = row->len;
              }
              
              if(ai->resn[0])
                UtilConcatVLA(&row->txt,&row->len,ai->resn);
              else
                UtilConcatVLA(&row->txt,&row->len,"''");
              
              r1->stop = row->len;
              
              UtilConcatVLA(&row->txt,&row->len," ");
            } else {
              UtilConcatVLA(&row->txt,&row->len,abbr);
              r1->stop = row->len;
            }
            if(default_color<0)
              r1->color = FindColor(ai,obj->NAtom-a);
            else
              r1->color = default_color;
            nCol++;
            last_abbr=abbr[0];
          }
          
          break;
        case 1: /* explicit residue codes */
          if(!AtomInfoSameResidueP(last,ai)) {
            last = ai;

            VLACheck(row->col,CSeqCol,nCol);
            r1 = row->col+nCol;
            r1->start = row->len;
            if(obj->DiscreteFlag) 
              r1->state = ai->discrete_state;
            first_atom_in_label = true;

            if(ai->resn[0])
              UtilConcatVLA(&row->txt,&row->len,ai->resn);
            else
              UtilConcatVLA(&row->txt,&row->len,"''");
            r1->stop = row->len;
            if(default_color<0)
              r1->color = FindColor(ai,obj->NAtom-a);
            else
              r1->color = default_color;
            UtilConcatVLA(&row->txt,&row->len," ");
            nCol++;
          }
          break;
        case 2: /* atom names */
          VLACheck(row->col,CSeqCol,nCol);
          r1 = row->col+nCol;
          r1->start = row->len;
          first_atom_in_label = true;
          if(ai->name[0])
            UtilConcatVLA(&row->txt,&row->len,ai->name);
          else
            UtilConcatVLA(&row->txt,&row->len,"''");
          r1->stop = row->len;
          if(default_color<0)
            r1->color = ai->color;
          else
            r1->color = default_color;
          UtilConcatVLA(&row->txt,&row->len," ");
          nCol++;
          break;
        case 3:
          if(!AtomInfoSameChainP(last,ai)) {
            last = ai;

            VLACheck(row->col,CSeqCol,nCol);
            r1 = row->col+nCol;
            r1->start = row->len;
            first_atom_in_label = true;

            if(ai->chain[0])
              UtilConcatVLA(&row->txt,&row->len,ai->chain);
            else
              UtilConcatVLA(&row->txt,&row->len,"''");
            r1->stop = row->len;
            if(default_color<0)
              r1->color = FindColor(ai,obj->NAtom-a);
            else
              r1->color = default_color;
            UtilConcatVLA(&row->txt,&row->len," ");
            nCol++;
          }
          break;
        case 4: /* state names */
          if(obj->DiscreteFlag) {
            CoordSet *cs;
            if((cs = obj->DiscreteCSet[a])!=last_disc) {
              last_disc = cs;
              if(cs) {
                default_color = SettingGet_i(cs->Setting,obj->Obj.Setting,
                                             cSetting_seq_view_color);
                VLACheck(row->col,CSeqCol,nCol);
                r1 = row->col+nCol;
                r1->start = row->len;
                r1->color = default_color;
                first_atom_in_label = true;
                
                if(cs->Name[0])
                  UtilConcatVLA(&row->txt,&row->len,cs->Name);
                else
                  UtilConcatVLA(&row->txt,&row->len,"''");
                r1->stop = row->len;
                r1->state = ai->discrete_state;
                UtilConcatVLA(&row->txt,&row->len," ");
                nCol++;
              }
            }
          } else { 
            /* non-discrete objects simply get their states enumerated
               without selections */
            
            if(last_state<0) {
              int b;
              CoordSet *cs;
              WordType buf1;
              last_state = 1;
              first_atom_in_label = true;
              for(b=0;b<obj->NCSet;b++) {
                cs = obj->CSet[b];
                if(cs) {
                  default_color = SettingGet_i(cs->Setting,obj->Obj.Setting,
                                               cSetting_seq_view_color);
                  
                  VLACheck(row->col,CSeqCol,nCol);
                  r1 = row->col+nCol;
                  r1->state = b+1;
                  r1->start = row->len;
                  r1->atom_at = nListEntries + 1; /* tricky & dangerous */
                  r1->color = default_color;
                  if(cs->Name[0])
                    UtilConcatVLA(&row->txt,&row->len,cs->Name);
                  else {
                    sprintf(buf1,"%d",b+1);
                    UtilConcatVLA(&row->txt,&row->len,buf1);
                  }
                  r1->stop = row->len;
                  UtilConcatVLA(&row->txt,&row->len," ");
                  nCol++;
                }
              }
            }
          }
          break;
        }

        if(first_atom_in_label) {
          if(nCol>1)  { /* terminate current list, if any */
            VLACheck(row->atom_lists,int,nListEntries);
            row->atom_lists[nListEntries]=-1;
            nListEntries++;
          }
          if(r1) {
            r1->atom_at = nListEntries;
          }
        }
        
        VLACheck(row->atom_lists,int,nListEntries);
        row->atom_lists[nListEntries] = a;
        nListEntries++;
        ai++;
      }

      if(lab) {
        /*        if(lab->len<row->len) {
          lab->len = row->len;
          }*/
        VLASize(lab->txt,char,lab->len+1);
        lab->txt[lab->len] = 0;
        VLACheck(lab->col,CSeqCol,nCol); /* make sure we've got column records for labels too */
        lab->nCol = nCol;

        /*if(row->len<lab->len) {
          row->len = lab->len;
          }*/
      }

      VLASize(row->txt,char,row->len+1);
      row->txt[row->len]=0;

      row->nCol = nCol;

      /* terminate last atom list */
      VLACheck(row->atom_lists,int,nListEntries);
      row->atom_lists[nListEntries] = -1;
      nListEntries++;
      nRow++;
    }
  }

  /* SECOND PASS: align columns to reflect current alignment and fixed labels */
  if(nRow)
    {
    int a,b;
    int nCol;
    int maxCol = 0;
    int done_flag = false;
    /* find out the maximum number of columns */

    for(a=0;a<nRow;a++) {
      row = row_vla + a;
      nCol = row->nCol;
      row->accum = 0; /* initialize the accumulators */
      row->current = 0;
      if(maxCol<nCol)
        maxCol = nCol;
    }

    /* in the simplest mode, just start each sequence in the same column */

    switch(0) {
    case 0:
      b = 0;
      while(!done_flag) {
        int max_offset = 0;
        done_flag = true;
        for(a=0;a<nRow;a++) {
          row = row_vla + a;
          if(!row->label_flag) {
            if(b< row->nCol) {
              CSeqCol *r1 = row->col + b;
              done_flag = false;
              
              r1->offset = r1->start + row->accum;
              if(max_offset<r1->offset)
                max_offset = r1->offset;
            }
          }
        }
        for(a=0;a<nRow;a++) { 
          row = row_vla + a;
          if(!row->label_flag) {
            if(b<row->nCol) {
              CSeqCol *r1 = row->col + b;
              if(b<3) {
                if(r1->offset<max_offset) {
                  row->accum += max_offset - r1->offset;
                }
              }
              r1->offset = r1->start + row->accum;
            }
          }
        }
        b++;
      }
      break;
    }
    

    for(a=0;a<nRow;a++) {
      row = row_vla + a;
      nCol = row->nCol;
      if(row->label_flag)
        lab=row;
      else {
        for(b=0;b<nCol;b++) {
          CSeqCol *r1 = row->col + b,*l1=NULL;
          if(lab) { 
            l1 = lab->col + b; /* if a fixed label is present, get the final offset from the residue line */
            if(l1->stop) 
              l1->offset = r1->offset;
          }
        }
        lab = NULL;
      }
    }
    
    }

  /* THIRD PASS: fill in labels, based on actual residue spacing */

  if(nRow&&(codes!=4)) {
    int a,b,c;
    int nCol;
    for(a=0;a<nRow;a++) {
      lab = row_vla + a;

      if(lab->label_flag) {
        int next_open = 0;
        int *atom_list;
        int st_len;
        int div,sub;
        int draw_it;
        int n_skipped = 0;
        int last_resv = -1;
        AtomInfoType *last_ai = NULL;
        ObjectMolecule *obj;
        AtomInfoType *ai;
        row = lab+1;
        nCol = row->nCol;
        obj = row->obj;
        div = SettingGet_i(obj->Obj.Setting,NULL,cSetting_seq_view_label_spacing);
        sub = SettingGet_i(obj->Obj.Setting,NULL,cSetting_seq_view_label_start);
        for(b=0;b<nCol;b++) {
          CSeqCol *r1 = row->col + b;
          CSeqCol *l1 = lab->col + b;

          ai = NULL;
          if(r1->atom_at) {
            atom_list = row->atom_lists + r1->atom_at;
            if(*atom_list>=0)
              ai = obj->AtomInfo + (*atom_list); /* get first atom in list */
          }
          if(l1->stop) {/* if label is already present, just line it up */
            l1->offset = r1->offset;
          } else if((r1->offset >= next_open)&&ai) {
            if((div>1)&&(codes!=2)) {
              if(! ((ai->resv-sub) % div))
                draw_it = true;
              else
                draw_it = false;
            } else {
              draw_it = true;
            }
            if(ai->resv!=(last_resv+1)) /* gap in sequence?  then draw label ASAP */
              draw_it = true;
            if(n_skipped >= (div+div)) /* don't skip too many without a label! */
              draw_it = true;

            if(AtomInfoSameResidueP(last_ai,ai)) /* don't ever draw a residue label twice */
              draw_it = false;

            if(draw_it) {
              n_skipped = 0;
              last_ai = ai;
              l1->start = lab->len;
              UtilConcatVLA(&lab->txt,&lab->len,ai->resi);
              l1->stop = lab->len;
              st_len = l1->stop - l1->start + 1;
              l1->offset = r1->offset;
              next_open = r1->offset + st_len;
              
              /* make sure this label doesn't conflict with a fixed label */
              
              for(c=b+1;c<nCol;c++) { 
                CSeqCol *l2 = lab->col + c;   
                if(l2->offset&&(l2->offset<next_open)) {
                  l1->start=0;
                  l1->stop=0;
                  break;
                }
                if((c-b)>st_len) /* only search as many columns as characters */
                  break;
              }
            } else
              n_skipped++;
          }

          if(ai)
            last_resv = ai->resv;
        }
      }
    }
  }

  /* FOURTH PASS: simply fill in character offsets */
  if(nRow) {
    int a,b;
    int nCol;
    int start,stop;
    for(a=0;a<nRow;a++) {
      row = row_vla + a;
      row->ext_len = 0;

      if(!row->label_flag) {
        nCol = row->nCol;

        for(b=0;b<nCol;b++) {
          CSeqCol *r1 = row->col + b;
          stop = r1->offset + (r1->stop-r1->start);
          if(row->ext_len<stop)
            row->ext_len = stop;
        }
        VLACheck(row->char2col,int,row->ext_len);
        UtilZeroMem(row->char2col,row->ext_len);
        for(b=0;b<nCol;b++) {
          CSeqCol *r1 = row->col + b;
          int c;
          start = r1->offset;
          stop = r1->offset + (r1->stop-r1->start);
          for(c=start;c<stop;c++) 
            row->char2col[c]=b+1;
        }
      }
    }
  }
  Seeker.handler.fClick = SeekerClick;
  Seeker.handler.fRelease = SeekerRelease;
  Seeker.handler.fDrag = SeekerDrag;
  Seeker.handler.fRefresh = SeekerRefresh;
  SeqSetRowVLA(row_vla,nRow);
  SeqSetHandler(&Seeker.handler);
}

void SeekerInit(void)
{
  CSeeker *I = &Seeker;  
  UtilZeroMem(I,sizeof(CSeeker));
  I->drag_row = -1;
  I->LastClickTime = UtilGetSeconds() - 1.0F;
}

void SeekerFree(void)
{
}



/*
  MaxEgg.cpp 
  Created by Steven "Sauce" Osman, 01/??/03
  odified and maintained by Ken Strickland, (02/25/03)-(Present)
  Carnegie Mellon University, Entetainment Technology Center

  This file implements the classes that are used in the Panda 3D file 
  exporter for 3D Studio Max.
*/

// Discreet-Generated ID for this app.
#define MNEG Logger::ST_MAP_ME_TO_APP_SPECIFIC_SYSTEM2
#define MNEG_GEOMETRY_GENERATION Logger::ST_MAP_ME_TO_APP_SPECIFIC_SYSTEM3

//Disable the forcing int to true or false performance warning
#pragma warning(disable: 4800)

/* MaxEggPluginClassDesc - A class that describes 3DS Plugin support.
   This basically says "Yes, I am a helper object!"
*/
class MaxEggPluginClassDesc : public ClassDesc 
{
public:
  int          IsPublic() { return TRUE; }
  void         *Create(BOOL loading = FALSE) { return new MaxEggPlugin(); }
  const TCHAR  *ClassName() { return GetString(IDS_CLASS_NAME); }
  SClass_ID    SuperClassID() { return HELPER_CLASS_ID; }
  Class_ID     ClassID() { return MaxEggPlugin_CLASS_ID; }
  const TCHAR  *Category() { return GetString(IDS_CATEGORY); }
  // returns fixed parsable name (scripter-visible name)
  const TCHAR *InternalName() { return _T("MaxEggPlugin"); }
};

// Our private global instance of the above class
static MaxEggPluginClassDesc MaxEggPluginDesc;

// Called by LibClassDesc to find out what the plugin is
ClassDesc* GetMaxEggPluginDesc() { return &MaxEggPluginDesc; }

// Initialize class-static variables
Mesh MaxEggPlugin::mesh;
short MaxEggPlugin::meshBuilt=0;
HWND MaxEggPlugin::hMaxEggParams = NULL;
IObjParam *MaxEggPlugin::iObjParams;

/* MaxEggPluginOptionsDlgProc() - This is the callback function for the
   dialog box that appears at the beginning of the conversion process.
 */

BOOL CALLBACK MaxEggPluginOptionsDlgProc( HWND hWnd, UINT message, 
                                          WPARAM wParam, LPARAM lParam ) 
{
  MaxOptionsDialog *tempEgg;
  int sel, res;

  //We pass in our plugin through the lParam variable. Let's convert it back.
  MaxEggPlugin *imp = (MaxEggPlugin*)GetWindowLongPtr(hWnd,GWLP_USERDATA); 
  if ( !imp && message != WM_INITDIALOG ) return FALSE;

  switch(message) 
    {
    // When we start, center the window.
    case WM_INITDIALOG:
      // this line is very necessary to pass the plugin as the lParam
      SetWindowLongPtr(hWnd,GWLP_USERDATA,lParam); 
      SetDlgFont( hWnd, imp->iObjParams->GetAppHFont() );
      MaxEggPlugin::hMaxEggParams = hWnd;
      return TRUE; break;
    
    case WM_MOUSEACTIVATE:
      imp->iObjParams->RealizeParamPanel();
      return TRUE; break;

    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MOUSEMOVE:
      imp->iObjParams->RollupMouseMessage(hWnd,message,wParam,lParam);
      return TRUE; break;
     
    // A control was modified
    case WM_COMMAND:
      //The modified control is found in the lower word of the wParam long.
      switch( LOWORD(wParam) ) {
        case IDC_OVERWRITE_CHECK:
          imp->autoOverwrite = 
            (IsDlgButtonChecked(hWnd, IDC_OVERWRITE_CHECK) == BST_CHECKED);
          return TRUE; break;
        case IDC_PVIEW_CHECK:
          imp->pview = 
            (IsDlgButtonChecked(hWnd, IDC_PVIEW_CHECK) == BST_CHECKED);
          return TRUE; break;
        case IDC_LOGGING:
          imp->logOutput = 
            (IsDlgButtonChecked(hWnd, IDC_LOGGING) == BST_CHECKED);
          return TRUE; break;
        case IDC_ADD_EGG:
          tempEgg = new MaxOptionsDialog();
          tempEgg->SetMaxInterface(imp->iObjParams);
          tempEgg->SetAnimRange();
          res = DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_EGG_DETAILS), 
                               hWnd, MaxOptionsDialogProc, (LPARAM)tempEgg);
          if (res == TRUE) {
            imp->SaveCheckState();
            imp->AddEgg(tempEgg);
            imp->UpdateUI();
          }
          else delete tempEgg;
          return TRUE; break;
        case IDC_EDIT_EGG:
          sel = ListView_GetSelectionMark(GetDlgItem(hWnd, IDC_LIST_EGGS));
          if (sel != -1) {
            MaxOptionsDialog *tempEgg = imp->GetEgg(sel);
            if (tempEgg) {
                tempEgg->SetAnimRange();
                tempEgg->CullBadNodes();
                DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_EGG_DETAILS), 
                               hWnd, MaxOptionsDialogProc, (LPARAM)tempEgg);
            }
            imp->SaveCheckState();
            imp->UpdateUI();
          }
          return TRUE; break;
        case IDC_REMOVE_EGG:
          sel = ListView_GetSelectionMark(GetDlgItem(hWnd, IDC_LIST_EGGS));
          if (sel != -1) {
            imp->SaveCheckState();
            imp->RemoveEgg(sel);
            imp->UpdateUI();
          }
          return TRUE; break;
        case IDC_EXPORT:
          imp->DoExport();
          return TRUE; break;
      }
    }
  return FALSE;
}

MaxEggPlugin::MaxEggPlugin() :
autoOverwrite(false), pview(true), logOutput(false), numEggs(0), maxEggs(5)
{
    eggList = new MaxOptionsDialog*[maxEggs];
    BuildMesh();
}

MaxEggPlugin::~MaxEggPlugin() {
    for (int i = 0; i < numEggs; i++) delete eggList[i];
    delete [] eggList;
}

void MaxEggPlugin::AddEgg(MaxOptionsDialog *newEgg) {
    if (numEggs >= maxEggs) {
        MaxOptionsDialog **newList;
        maxEggs *= 2;
        newList = new MaxOptionsDialog*[maxEggs];
        for (int i = 0; i < numEggs; i++) newList[i] = eggList[i];
        delete [] eggList;
        eggList = newList;
    }
    
    eggList[numEggs++] = newEgg;
}

void MaxEggPlugin::RemoveEgg(int i) {
    if (i >= 0 && i < numEggs) {
        delete eggList[i];
        for (int j = i+1; j < numEggs;) eggList[i++] = eggList[j++];
        --numEggs;
    }
}

void MaxEggPlugin::BeginEditParams( IObjParam *ip, ULONG flags,Animatable *prev )
{
    iObjParams = ip;
    for (int i=0; i<numEggs; i++) {
        eggList[i]->SetMaxInterface(ip);
    }
    
    if ( !hMaxEggParams ) {
        hMaxEggParams = ip->AddRollupPage(hInstance, 
                                          MAKEINTRESOURCE(IDD_PANEL),
                                          MaxEggPluginOptionsDlgProc, 
                                          GetString(IDS_PARAMS), 
                                          (LPARAM)this );
        ip->RegisterDlgWnd(hMaxEggParams);
    } else {
        SetWindowLongPtr( hMaxEggParams, GWLP_USERDATA, (LPARAM)this );
    }
    
    UpdateUI();
}

void MaxEggPlugin::EndEditParams( IObjParam *ip, ULONG flags,Animatable *prev)
{
    SaveCheckState();
    if ( flags&END_EDIT_REMOVEUI ) {
        ip->UnRegisterDlgWnd(hMaxEggParams);
        ip->DeleteRollupPage(hMaxEggParams);
        hMaxEggParams = NULL;
    } else {
        SetWindowLongPtr( hMaxEggParams, GWLP_USERDATA, NULL );
    }
}

void MaxEggPlugin::SaveCheckState() {
    if (!hMaxEggParams) return;
    HWND lv = GetDlgItem(hMaxEggParams, IDC_LIST_EGGS);
    for (int i = 0; i < numEggs; i++)
        eggList[i]->_checked = ListView_GetCheckState(lv, i);
}

void MaxEggPlugin::UpdateUI() {
    HWND lv = GetDlgItem(hMaxEggParams, IDC_LIST_EGGS);
    LV_COLUMN pCol;
    
    if (ListView_GetColumnWidth(lv, 1) <= 0 || ListView_GetColumnWidth(lv, 1) > 10000) {
        //Columns have not been setup, so initialize the control
        ListView_SetExtendedListViewStyleEx(lv, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT, 
                                            LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
        
        pCol.fmt = LVCFMT_LEFT;
        pCol.cx = 96;
        pCol.pszText = "Filename";
        pCol.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
        pCol.iSubItem = 0;
        ListView_InsertColumn(lv, 0, &pCol);
        
        pCol.cx = 44;
        pCol.pszText = "Type";
        ListView_InsertColumn(lv, 1, &pCol);
    }
    
    //Add the eggs to the list
    ListView_DeleteAllItems(lv);
    LV_ITEM Item;
    Item.mask = LVIF_TEXT;
    
    for (int i = 0; i < numEggs; i++) {
        Item.iItem = i;
        Item.iSubItem = 0;
        Item.pszText = eggList[i]->_short_name;
        ListView_InsertItem(lv, &Item);
        Item.iSubItem = 1;
        switch(eggList[i]->_anim_type) {
        case MaxEggOptions::AT_chan:  Item.pszText = "Animation"; break;
        case MaxEggOptions::AT_both:  Item.pszText = "Both"; break;
        case MaxEggOptions::AT_pose:  Item.pszText = "Pose"; break;
        case MaxEggOptions::AT_model: Item.pszText = "Model"; break;
        default:                      Item.pszText = "Model"; break;
        }
        ListView_SetItem(lv, &Item);
        ListView_SetCheckState(lv, i, eggList[i]->_checked);
    }
    
    // Set the "Overwrite Existing Files" and "Pview" checkboxes
    CheckDlgButton(hMaxEggParams, IDC_OVERWRITE_CHECK, 
                   autoOverwrite ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hMaxEggParams, IDC_PVIEW_CHECK, 
                   pview ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hMaxEggParams, IDC_LOGGING, 
                   logOutput ? BST_CHECKED : BST_UNCHECKED);
}

void MaxEggPlugin::DoExport() {
    int good = 0, bad = 0;
    
    std::stringstream status;
    
    SaveCheckState();
    
    for (int i = 0; i < numEggs; i++) {
        if (eggList[i]->_checked) {
            MaxToEggConverter converter;
            if (converter.convert((MaxEggOptions*)eggList[i])) {
                good += 1;
                status << "Successfully created " << eggList[i]->_short_name << ".egg\n";
            } else {
                bad += 1;
                status << "Could not export " << eggList[i]->_short_name << ".egg\n";
            }
        }
    }
    
    UINT mask = MB_OK;
    if (bad > 0) mask |= MB_ICONEXCLAMATION;
    else mask |= MB_ICONINFORMATION;
    
    MessageBox(hMaxEggParams, status.str().c_str(), "Panda3D Export results", mask);
    
    int pviewed = 0;
    if (pview && good > 0) {
        for (i = 0; i < numEggs; i++) {
            if (eggList[i]->_checked && eggList[i]->_successful) {
                if (eggList[i]->_anim_type != MaxEggOptions::AT_chan) {
                    char buf[1024];
                    PROCESS_INFORMATION pi;
                    STARTUPINFO si;
                    
                    memset(&si,0,sizeof(si));
                    si.cb= sizeof(si);
                    
                    sprintf(buf, "pview %s.egg?", eggList[i]->_short_name);
                    char cmdLine[2048];
                    sprintf(cmdLine, "pview \"%s\"", eggList[i]->_file_name);
                    CreateProcess(NULL, cmdLine, NULL, NULL, FALSE, CREATE_NEW_CONSOLE,
                                  NULL, NULL, &si, &pi);
                    pviewed += 1;
                }
            }
        }
    }
}


void MaxEggPlugin::BuildMesh()
{
    int i;
    if(meshBuilt) return;
    
    mesh.setNumVerts(252);
    mesh.setNumFaces(84);
    mesh.setSmoothFlags(0);
    mesh.setNumTVerts (0);
    mesh.setNumTVFaces (0);
    
    for (i=0; i<252; i++) 
        mesh.setVert(i, meshVerts[i][0]*10, meshVerts[i][1]*10, meshVerts[i][2]*10);
    for (i=0; i<84; i++) {
        mesh.faces[i].setEdgeVisFlags(1, 1, 0);
        mesh.faces[i].setSmGroup(0);
        mesh.faces[i].setVerts(i*3, i*3+1, i*3+2);
    }
    
    mesh.InvalidateGeomCache();
    mesh.BuildStripsAndEdges();
    
    meshBuilt = TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// The creation callback - sets the initial position of the helper in the scene.
///////////////////////////////////////////////////////////////////////////////////////////////////////

class MaxEggPluginCreateMouseCallBack: public CreateMouseCallBack 
{
public:
    int proc( ViewExp *vpt,int msg, int point, int flags, IPoint2 m, Matrix3& mat );
};

int MaxEggPluginCreateMouseCallBack::proc(ViewExp *vpt,int msg, int point, int flags, IPoint2 m, Matrix3& mat ) 
{
    if (msg==MOUSE_POINT||msg==MOUSE_MOVE) {
        switch(point) {
        case 0:
            mat.SetTrans(vpt->SnapPoint(m,m,NULL,SNAP_IN_PLANE));
            break;
        case 1:
            mat.SetTrans(vpt->SnapPoint(m,m,NULL,SNAP_IN_PLANE));
            if (msg==MOUSE_POINT) return CREATE_STOP;
            break;
        }
    } else if (msg == MOUSE_ABORT) {
        return CREATE_ABORT; 
    }
    return CREATE_CONTINUE;
}

static MaxEggPluginCreateMouseCallBack MaxEggCreateMouseCB;

CreateMouseCallBack* MaxEggPlugin::GetCreateMouseCallBack() 
{ return &MaxEggCreateMouseCB; }

///////////////////////////////////////////////////////////////////////////////////////////////////////
//Boilerplate functions for dealing with the display of the plugin
///////////////////////////////////////////////////////////////////////////////////////////////////////

void MaxEggPlugin::GetMat(TimeValue t, INode* inode, ViewExp* vpt, Matrix3& tm) 
{
    tm = inode->GetObjectTM(t);
    tm.NoScale();
    float scaleFactor = vpt->NonScalingObjectSize()*vpt->GetVPWorldWidth(tm.GetTrans())/(float)360.0;
    tm.Scale(Point3(scaleFactor,scaleFactor,scaleFactor));
}

void MaxEggPlugin::GetDeformBBox(TimeValue t, Box3& box, Matrix3 *tm, BOOL useSel )
{
    box = mesh.getBoundingBox(tm);
}

void MaxEggPlugin::GetLocalBoundBox(TimeValue t, INode* inode, ViewExp* vpt, Box3& box ) 
{
    Matrix3 m = inode->GetObjectTM(t);
    Point3 pt;
    Point3 q[4];
    float scaleFactor = vpt->GetVPWorldWidth(m.GetTrans())/360.0f;
    box = mesh.getBoundingBox();
    box.Scale(scaleFactor);
}

void MaxEggPlugin::GetWorldBoundBox(TimeValue t, INode* inode, ViewExp* vpt, Box3& box )
{
    int i, nv; Matrix3 tm; Point3 pt;
    GetMat(t,inode,vpt,tm);
    nv = mesh.getNumVerts();
    box.Init();
    for (i=0; i<nv; i++) 
        box += tm*mesh.getVert(i);
}

int MaxEggPlugin::HitTest(TimeValue t, INode *inode, int type, int crossing, int flags, IPoint2 *p, ViewExp *vpt) 
{
    HitRegion hitRegion;
    DWORD savedLimits;
    Matrix3 m;
    GraphicsWindow *gw = vpt->getGW();
    Material *mtl = gw->getMaterial();
    MakeHitRegion(hitRegion,type,crossing,4,p);
    gw->setRndLimits(((savedLimits = gw->getRndLimits()) | GW_PICK) & ~GW_ILLUM);
    GetMat(t,inode,vpt,m);
    gw->setTransform(m);
    gw->clearHitCode();
    if (mesh.select( gw, mtl, &hitRegion, flags & HIT_ABORTONHIT )) 
        return TRUE;
    return FALSE;
}

int MaxEggPlugin::Display(TimeValue t, INode* inode, ViewExp *vpt, int flags) 
{
    Matrix3 m;
    GraphicsWindow *gw = vpt->getGW();
    Material *mtl = gw->getMaterial();
    
    GetMat(t,inode,vpt,m);
    gw->setTransform(m);
    DWORD rlim = gw->getRndLimits();
    gw->setRndLimits(GW_WIREFRAME|GW_BACKCULL);
    if (inode->Selected()) 
        gw->setColor( LINE_COLOR, GetSelColor());
    else if(!inode->IsFrozen())
        gw->setColor( LINE_COLOR, GetUIColor(COLOR_TAPE_OBJ));
    mesh.render( gw, mtl, NULL, COMP_ALL);
    return 0;
}

RefResult MaxEggPlugin::NotifyRefChanged(Interval changeInt, RefTargetHandle hTarget, PartID& partID, RefMessage message ) 
{
    UpdateUI();
    return REF_SUCCEED;
}

ObjectState MaxEggPlugin::Eval(TimeValue time)
{
    return ObjectState(this);
}

Interval MaxEggPlugin::ObjectValidity(TimeValue t) 
{
    Interval ivalid;
    ivalid.SetInfinite();
    return ivalid;
}

RefTargetHandle MaxEggPlugin::Clone(RemapDir& remap) 
{
    MaxEggPlugin* newob = new MaxEggPlugin();
    return(newob);
}

///////////////////////////////////////////////////////////
// Loading and saving the plugin
///////////////////////////////////////////////////////////

IOResult MaxEggPlugin::Save(ISave *isave) {
    SaveCheckState();
    for (int i = 0; i < numEggs; i++)
        eggList[i]->Save(isave);
    ChunkSave(isave,   CHUNK_OVERWRITE_FLAG,  autoOverwrite);
    ChunkSave(isave,   CHUNK_PVIEW_FLAG,      pview);
    ChunkSave(isave,   CHUNK_LOG_OUTPUT,      logOutput);
    
    return IO_OK;
}

IOResult MaxEggPlugin::Load(ILoad *iload) {
    IOResult res = iload->OpenChunk();
    MaxOptionsDialog *temp;
    
    while (res == IO_OK) {
        switch(iload->CurChunkID()) {
        case CHUNK_OVERWRITE_FLAG: autoOverwrite = ChunkLoadBool(iload); break;
        case CHUNK_PVIEW_FLAG:     pview = ChunkLoadBool(iload); break;
        case CHUNK_LOG_OUTPUT:     logOutput = ChunkLoadBool(iload); break;
        case CHUNK_EGG_EXP_OPTIONS:
            temp = new MaxOptionsDialog();
            temp->SetMaxInterface(iObjParams);
            temp->Load(iload);
            AddEgg(temp);
            break;
        }
        iload->CloseChunk();
        res = iload->OpenChunk();
    }
    
    return IO_OK;
}

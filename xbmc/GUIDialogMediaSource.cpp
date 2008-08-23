/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include "GUIDialogMediaSource.h"
#include "GUIDialogKeyboard.h"
#include "GUIDialogFileBrowser.h"
#include "GUIDialogContentSettings.h"
#include "GUIWindowVideoFiles.h"
#include "GUIWindowManager.h"
#include "Util.h"
#include "FileSystem/PluginDirectory.h"
#include "GUIDialogYesNo.h"
#include "FileSystem/File.h"
#include "FileItem.h"

using namespace std;
using namespace DIRECTORY;

#define CONTROL_HEADING          2
#define CONTROL_PATH            10
#define CONTROL_PATH_BROWSE     11
#define CONTROL_NAME            12
#define CONTROL_PATH_ADD        13
#define CONTROL_PATH_REMOVE     14
#define CONTROL_OK              18
#define CONTROL_CANCEL          19
#define CONTROL_CONTENT         20

CGUIDialogMediaSource::CGUIDialogMediaSource(void)
    : CGUIDialog(WINDOW_DIALOG_MEDIA_SOURCE, "DialogMediaSource.xml")
{
  m_paths =  new CFileItemList;
}

CGUIDialogMediaSource::~CGUIDialogMediaSource()
{
  delete m_paths;
}

bool CGUIDialogMediaSource::OnAction(const CAction &action)
{
  if (action.wID == ACTION_PREVIOUS_MENU)
    m_confirmed = false;
  return CGUIDialog::OnAction(action);
}

bool CGUIDialogMediaSource::OnMessage(CGUIMessage& message)
{
  switch ( message.GetMessage() )
  {
  case GUI_MSG_CLICKED:
    {
      int iControl = message.GetSenderId();
      int iAction = message.GetParam1();
      if (iControl == CONTROL_PATH && iAction == ACTION_SELECT_ITEM || iAction == ACTION_MOUSE_LEFT_CLICK)
        OnPath(GetSelectedItem());
      else if (iControl == CONTROL_PATH_BROWSE)
        OnPathBrowse(GetSelectedItem());
      else if (iControl == CONTROL_PATH_ADD)
        OnPathAdd();
      else if (iControl == CONTROL_PATH_REMOVE)
        OnPathRemove(GetSelectedItem());
      else if (iControl == CONTROL_NAME)
        OnName();
      else if (iControl == CONTROL_OK)
        OnOK();
      else if (iControl == CONTROL_CANCEL)
        OnCancel();
      else if (iControl == CONTROL_CONTENT)
      {
        CMediaSource share;
        share.FromNameAndPaths("video", m_name, GetPaths());
        
        CGUIDialogContentSettings::ShowForDirectory(share.strPath,m_info,m_settings,m_bRunScan);
      }
      return true;
    }
    break;
  case GUI_MSG_WINDOW_INIT:
    {
      m_confirmed = false;
      m_bRunScan = false;
      m_settings.parent_name = false;
      m_settings.recurse = 0;
      UpdateButtons();
    }
    break;
  case GUI_MSG_SETFOCUS:
    if (m_hasMultiPath)
    {
      if (message.GetControlId() == CONTROL_PATH_BROWSE ||
               message.GetControlId() == CONTROL_PATH_ADD ||
               message.GetControlId() == CONTROL_PATH_REMOVE)
      {
        HighlightItem(GetSelectedItem());
      }
      else
        HighlightItem(-1);
    }
    break;
  }
  return CGUIDialog::OnMessage(message);
}

// \brief Show CGUIDialogMediaSource dialog and prompt for a new media source.
// \return True if the media source is added, false otherwise.
bool CGUIDialogMediaSource::ShowAndAddMediaSource(const CStdString &type)
{
  CGUIDialogMediaSource *dialog = (CGUIDialogMediaSource *)m_gWindowManager.GetWindow(WINDOW_DIALOG_MEDIA_SOURCE);
  if (!dialog) return false;
  dialog->Initialize();
  dialog->SetShare(CMediaSource());
  dialog->SetTypeOfMedia(type);
  dialog->DoModal();
  bool confirmed(dialog->IsConfirmed());
  if (confirmed)
  { // yay, add this share
    CMediaSource share;
    unsigned int i,j=2;
    bool bConfirmed=false;
    VECSOURCES* pShares = g_settings.GetSourcesFromType(type);
    CStdString strName = dialog->m_name;
    while (!bConfirmed)
    {
      for (i=0;i<pShares->size();++i)
      {
        if ((*pShares)[i].strName.Equals(strName))
          break;
      }
      if (i < pShares->size()) // found a match -  try next
        strName.Format("%s (%i)",dialog->m_name,j++);
      else
        bConfirmed = true;
    }
    share.FromNameAndPaths(type, strName, dialog->GetPaths());
    if (dialog->m_paths->Size() > 0) {
      share.m_strThumbnailImage = dialog->m_paths->Get(0)->GetThumbnailImage();
    }
    g_settings.AddShare(type, share);

    if (type == "video")
    {
      if (dialog->m_bRunScan)
        CGUIWindowVideoBase::OnScan(share.strPath,dialog->m_info,dialog->m_settings);

    }
  }
  dialog->m_paths->Clear();
  return confirmed;
}

bool CGUIDialogMediaSource::ShowAndEditMediaSource(const CStdString &type, const CStdString&share)
{
  VECSOURCES* pShares=NULL;
  
  if (type.Equals("upnpmusic"))
    pShares = &g_settings.m_UPnPMusicSources;
  if (type.Equals("upnpvideo"))
    pShares = &g_settings.m_UPnPVideoSources;
  if (type.Equals("upnppictures"))
    pShares = &g_settings.m_UPnPPictureSources;

  if (pShares)
  {
    for (unsigned int i=0;i<pShares->size();++i)
    {
      if ((*pShares)[i].strName.Equals(share))
        return ShowAndEditMediaSource(type,(*pShares)[i]);
    }
  }

  return false;
}

bool CGUIDialogMediaSource::ShowAndEditMediaSource(const CStdString &type, const CMediaSource &share)
{
  CStdString strOldName = share.strName;
  CGUIDialogMediaSource *dialog = (CGUIDialogMediaSource *)m_gWindowManager.GetWindow(WINDOW_DIALOG_MEDIA_SOURCE);
  if (!dialog) return false;
  dialog->Initialize();
  dialog->SetShare(share);
  dialog->SetTypeOfMedia(type, true);
  dialog->DoModal();
  bool confirmed(dialog->IsConfirmed());
  if (confirmed)
  { // yay, add this share
    unsigned int i,j=2;
    bool bConfirmed=false;
    VECSOURCES* pShares = g_settings.GetSourcesFromType(type);
    CStdString strName = dialog->m_name;
    while (!bConfirmed)
    {
      for (i=0;i<pShares->size();++i)
      {
        if ((*pShares)[i].strName.Equals(strName))
          break;
      }
      if (i < pShares->size() && (*pShares)[i].strName != strOldName) // found a match -  try next
        strName.Format("%s (%i)",dialog->m_name,j++);
      else
        bConfirmed = true;
    }

    CMediaSource newShare;
    newShare.FromNameAndPaths(type, strName, dialog->GetPaths());
    g_settings.UpdateShare(type, strOldName, newShare);
  }
  dialog->m_paths->Clear();
  return confirmed;
}

void CGUIDialogMediaSource::OnPathBrowse(int item)
{
  if (item < 0 || item > m_paths->Size()) return;
  // Browse is called.  Open the filebrowser dialog.
  // Ignore current path is best at this stage??
  CStdString path;
  bool allowNetworkShares(m_type != "programs" && m_type.Left(4) != "upnp");
  VECSOURCES extraShares;

  if (m_type == "music" || m_type == "upnpmusic")
  { // add the music playlist location
    CMediaSource share1;
    share1.strPath = "special://musicplaylists/";
    share1.strName = g_localizeStrings.Get(20011);
    extraShares.push_back(share1);

    share1.strPath = "smb://";
    share1.strName = "Windows Network (SMB)";
    extraShares.push_back(share1);

    share1.strPath = "upnp://";
    share1.strName = "UPnP Devices";
    extraShares.push_back(share1);

    share1.strPath = "sap://";
    share1.strName = "SAP Streams";
    extraShares.push_back(share1);

    if (g_guiSettings.GetString("mymusic.recordingpath",false) != "")
    {
      CMediaSource share2;
      share2.strPath = "special://recordings/";
      share2.strName = g_localizeStrings.Get(20007);
      extraShares.push_back(share2);
    }
    if (g_guiSettings.GetString("cddaripper.path",false) != "")
    {
      CMediaSource share2;
      share2.strPath = "special://cdrips/";
      share2.strName = g_localizeStrings.Get(21883);
      extraShares.push_back(share2);
    }
#ifdef _XBOX
    share1.strPath = "soundtrack://";
    share1.strName = "MS Soundtracks";
    extraShares.push_back(share1);
#endif
    if (g_guiSettings.GetBool("network.enableinternet"))
    {
      CMediaSource share3;
      share3.strName = "Shoutcast";
      share3.strPath = "shout://www.shoutcast.com/sbin/newxml.phtml";
      extraShares.push_back(share3);

      if (g_guiSettings.GetString("lastfm.username") != "")
      {
        CMediaSource share4;
        share4.strName = "Last.FM";
        share4.strPath = "lastfm://";
        extraShares.push_back(share4);
      }
    }
    // add the plugins dir as needed
    if (CPluginDirectory::HasPlugins("music"))
    {
      CMediaSource share2;
      share2.strPath = "plugin://music/";
      share2.strName = g_localizeStrings.Get(1038); // Music Plugins
      extraShares.push_back(share2);
    }
 }
  else if (m_type == "video" || m_type == "upnpvideo")
  { // add the music playlist location
    CMediaSource share1;
    share1.strPath = "special://videoplaylists/";
    share1.strName = g_localizeStrings.Get(20012);
    extraShares.push_back(share1);

    CMediaSource share2;
    share2.strPath = "rtv://*/";
    share2.strName = "ReplayTV Devices";
    extraShares.push_back(share2);

    share2.strPath = "smb://";
    share2.strName = "Windows Network (SMB)";
    extraShares.push_back(share2);

    share2.strPath = "hdhomerun://";
    share2.strName = "HDHomerun Devices";
    extraShares.push_back(share2);

    share2.strPath = "sap://";
    share2.strName = "SAP Streams";
    extraShares.push_back(share2);

    share2.strPath = "upnp://";
    share2.strName = "UPnP Devices";
    extraShares.push_back(share2);

    // add the plugins dir as needed
    if (CPluginDirectory::HasPlugins("video"))
    {
      CMediaSource share3;
      share3.strPath = "plugin://video/";
      share3.strName = g_localizeStrings.Get(1037); // Video Plugins
      extraShares.push_back(share3);
    }
  }
  else if (m_type == "pictures" || m_type == "upnpictures")
  {
    if (g_guiSettings.GetString("pictures.screenshotpath",false)!= "")
    {
      CMediaSource share1;
      share1.strPath = "special://screenshots/";
      share1.strName = g_localizeStrings.Get(20008);
      extraShares.push_back(share1);
    }

    CMediaSource share2;

    share2.strPath = "smb://";
    share2.strName = "Windows Network (SMB)";
    extraShares.push_back(share2);

    share2.strPath = "upnp://";
    share2.strName = "UPnP Devices";
    extraShares.push_back(share2);

    // add the plugins dir as needed
    if (CPluginDirectory::HasPlugins("pictures"))
    {
      CMediaSource share2;
      share2.strPath = "plugin://pictures/";
      share2.strName = g_localizeStrings.Get(1039); // Picture Plugins
      extraShares.push_back(share2);
    }
  }
  else if (m_type == "programs")
  {
    if (CPluginDirectory::HasPlugins("programs"))
    {
      CMediaSource share2;
      share2.strPath = "plugin://programs/";
      share2.strName = g_localizeStrings.Get(1043); // Program Plugins
      extraShares.push_back(share2);
    }
  }
  if (CGUIDialogFileBrowser::ShowAndGetSource(path, allowNetworkShares, extraShares.size()==0?NULL:&extraShares))
  {
    m_paths->Get(item)->m_strPath = path;
    if (m_name.IsEmpty())
    {
      CURL url(path);
      url.GetURLWithoutUserDetails(m_name);
      CUtil::RemoveSlashAtEnd(m_name);
      m_name = CUtil::GetTitleFromPath(m_name);
    }
    UpdateButtons();
  }
}

void CGUIDialogMediaSource::OnPath(int item)
{
  if (item < 0 || item > m_paths->Size()) return;
  CGUIDialogKeyboard::ShowAndGetInput(m_paths->Get(item)->m_strPath, g_localizeStrings.Get(1021), false);
  UpdateButtons();
}

void CGUIDialogMediaSource::OnName()
{
  CGUIDialogKeyboard::ShowAndGetInput(m_name, g_localizeStrings.Get(1022), false);
  UpdateButtons();
}

void CGUIDialogMediaSource::OnOK()
{
  // verify the path by doing a GetDirectory.
  CFileItemList items;

  CMediaSource share;
  share.FromNameAndPaths(m_type, m_name, GetPaths());
  // hack: Need to temporarily add the share, then get path, then remove share
  VECSOURCES *shares = g_settings.GetSourcesFromType(m_type);
  if (shares)
    shares->push_back(share);
  if (share.strPath.Left(9).Equals("plugin://") || CDirectory::GetDirectory(share.strPath, items, "", false, true) || CGUIDialogYesNo::ShowAndGetInput(1001,1025,1003,1004))
  {
    if (share.strPath.Left(9).Equals("plugin://"))
    {
      CStdString strPath=share.strPath;
      strPath.Replace("plugin://","Q:\\plugins\\");
      strPath.Replace("/","\\");
      CFileItem item(strPath,true);
      item.SetCachedProgramThumb();
      if (!item.HasThumbnail())
        item.SetUserProgramThumb();
      if (!item.HasThumbnail())
      {
        CUtil::AddFileToFolder(strPath,"default.py",item.m_strPath);
        item.m_bIsFolder = false;
        item.SetCachedProgramThumb();
        if (!item.HasThumbnail())
          item.SetUserProgramThumb();
      }
      if (item.HasThumbnail())
      {
        CFileItem item2(share.strPath,true);
        XFILE::CFile::Cache(item.GetThumbnailImage(),item2.GetCachedProgramThumb());
        m_paths->Get(0)->SetThumbnailImage(item2.GetCachedProgramThumb());
      }
    }
    m_confirmed = true;
    Close();
  }
  // and remove the share again
  if (shares)
    shares->erase(--shares->end());
}

void CGUIDialogMediaSource::OnCancel()
{
  m_confirmed = false;
  Close();
}

void CGUIDialogMediaSource::UpdateButtons()
{
  if (m_paths->Get(0)->m_strPath.IsEmpty() || m_name.IsEmpty())
  {
    CONTROL_DISABLE(CONTROL_OK)
  }
  else
  {
    CONTROL_ENABLE(CONTROL_OK)
  }
  if (m_paths->Size() <= 1)
  {
    CONTROL_DISABLE(CONTROL_PATH_REMOVE)
  }
  else
  {
    CONTROL_ENABLE(CONTROL_PATH_REMOVE)
  }
  // name
  SET_CONTROL_LABEL(CONTROL_NAME, m_name)

  if (m_hasMultiPath)
  {
    int currentItem = GetSelectedItem();
    CGUIMessage msgReset(GUI_MSG_LABEL_RESET, GetID(), CONTROL_PATH);
    OnMessage(msgReset);
    for (int i = 0; i < m_paths->Size(); i++)
    {
      CFileItemPtr item = m_paths->Get(i);
      CStdString path;
      CURL url(item->m_strPath);
      url.GetURLWithoutUserDetails(path);
      if (path.IsEmpty()) path = "<"+g_localizeStrings.Get(231)+">"; // <None>
      item->SetLabel(path);
      CGUIMessage msg(GUI_MSG_LABEL_ADD, GetID(), CONTROL_PATH, 0, 0, item);
      OnMessage(msg);
    }
    CGUIMessage msg(GUI_MSG_ITEM_SELECT, GetID(), CONTROL_PATH, currentItem);
    OnMessage(msg);
  }
  else
  {
    CStdString path;
    CURL url(m_paths->Get(0)->m_strPath);
    url.GetURLWithoutUserDetails(path);
    if (path.IsEmpty()) path = "<"+g_localizeStrings.Get(231)+">"; // <None>
    SET_CONTROL_LABEL(CONTROL_PATH, path)
  }

  if (m_type.Equals("video"))
  {
    SET_CONTROL_VISIBLE(CONTROL_CONTENT)
    if (m_paths->Get(0)->m_strPath.IsEmpty() || m_name.IsEmpty())
    {
      CONTROL_DISABLE(CONTROL_CONTENT)
    }
    else
    {
      CONTROL_ENABLE(CONTROL_CONTENT)
    }
  }
  else
  {
    SET_CONTROL_HIDDEN(CONTROL_CONTENT)
  }
}

void CGUIDialogMediaSource::SetShare(const CMediaSource &share)
{
  m_paths->Clear();
  for (unsigned int i = 0; i < share.vecPaths.size(); i++)
  {
    CFileItemPtr item(new CFileItem(share.vecPaths[i], true));
    m_paths->Add(item);
  }
  if (0 == share.vecPaths.size())
  {
    CFileItemPtr item(new CFileItem("", true));
    m_paths->Add(item);
  }
  m_name = share.strName;
  UpdateButtons();
}

void CGUIDialogMediaSource::SetTypeOfMedia(const CStdString &type, bool editNotAdd)
{
  m_type = type;
  int typeStringID = -1;
  if (type == "music")
    typeStringID = 249; // "Music"
  else if (type == "video")
    typeStringID = 291;  // "Video"
  else if (type == "programs")
    typeStringID = 350;  // "Programs"
  else if (type == "pictures")
    typeStringID = 1213;  // "Pictures"
  else if (type == "upnpmusic")
    typeStringID = 21356;
  else if (type == "upnpvideo")
    typeStringID = 21357;
  else if (type == "upnppictures")
    typeStringID = 21358;
  else // if (type == "files");
    typeStringID = 744;  // "Files"
  CStdString format;
  format.Format(g_localizeStrings.Get(editNotAdd ? 1028 : 1020).c_str(), g_localizeStrings.Get(typeStringID).c_str());
  SET_CONTROL_LABEL(CONTROL_HEADING, format);
}

void CGUIDialogMediaSource::OnWindowLoaded()
{
  CGUIDialog::OnWindowLoaded();
  // disable the spincontrol
#ifdef PRE_SKIN_VERSION_2_1_COMPATIBILITY
  const CGUIControl *control = GetControl(CONTROL_PATH);
  if (control && control->GetControlType() == CGUIControl::GUICONTAINER_LIST)
  {
    CGUIControl *spin = (CGUIControl *)GetControl(CONTROL_PATH + 5000);
    if (spin)
      spin->SetVisible(false);
    m_hasMultiPath = true;
  }
  else
    m_hasMultiPath = false;
#else
  m_hasMultiPath = true;
#endif
}

int CGUIDialogMediaSource::GetSelectedItem()
{
  if (!m_hasMultiPath)
    return 0;

  CGUIMessage message(GUI_MSG_ITEM_SELECTED, GetID(), CONTROL_PATH);
  OnMessage(message);
  int value = message.GetParam1();
  if (value < 0 || value > m_paths->Size()) return 0;
  return value;
}

void CGUIDialogMediaSource::HighlightItem(int item)
{
  for (int i = 0; i < m_paths->Size(); i++)
    m_paths->Get(i)->Select(false);
  if (item >= 0 && item < m_paths->Size())
    m_paths->Get(item)->Select(true);
  CGUIMessage msg(GUI_MSG_ITEM_SELECT, GetID(), CONTROL_PATH, item);
  OnMessage(msg);
}

void CGUIDialogMediaSource::OnPathRemove(int item)
{
  m_paths->Remove(item);
  UpdateButtons();
  if (item >= m_paths->Size())
    HighlightItem(m_paths->Size() - 1);
  else
    HighlightItem(item);
  if (m_paths->Size() <= 1)
  {
    SET_CONTROL_FOCUS(CONTROL_PATH_ADD, 0);
  }
}

void CGUIDialogMediaSource::OnPathAdd()
{
  // add a new item and select it as well
  CFileItemPtr item(new CFileItem("", true));
  m_paths->Add(item);
  UpdateButtons();
  HighlightItem(m_paths->Size() - 1);
}

vector<CStdString> CGUIDialogMediaSource::GetPaths()
{
  vector<CStdString> paths;
  for (int i = 0; i < m_paths->Size(); i++)
    paths.push_back(m_paths->Get(i)->m_strPath);
  return paths;
}


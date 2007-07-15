/*  Misfit Model 3D
 * 
 *  Copyright (c) 2004-2007 Kevin Worcester
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
 *  USA.
 *
 *  See the COPYING file for full license text.
 */

#ifndef __METAWIN_H
#define __METAWIN_H

#include "metawin.base.h"

#include <qcheckbox.h>
#include <qradiobutton.h>

#include "mq3macro.h"
#include "mq3compat.h"

class Model;
class QListViewItem;

class MetaWindow : public MetaWindowBase
{
      Q_OBJECT
   public:
      MetaWindow( Model *, QWidget * parent = NULL, const char * name = "" );
      virtual ~MetaWindow();

   public slots:
      void helpNowEvent( int );

      void newClicked();
      void deleteClicked();
      void editItemEvent( QListViewItem * item );

      void accept();
      void reject();

   protected:
      QAccel * m_accel;
      Model  * m_model;
};

#endif // __METAWIN_H


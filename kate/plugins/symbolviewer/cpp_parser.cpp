/***************************************************************************
                          cpp_parser.cpp  -  description
                             -------------------
    begin                : Apr 2 2003
    author               : 2003 Massimo Callegari
    email                : massimocallegari@yahoo.it
 ***************************************************************************/
 /***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "plugin_katesymbolviewer.h"

void KatePluginSymbolViewerView::parseCppSymbols(void)
{
  if (!win->activeView())
   return;

 QString cl; // Current Line
 QString stripped;
 int i, j, tmpPos = 0;
 int par = 0, graph = 0, retry = 0;
 char block = 0, comment = 0, macro = 0, macro_pos = 0;
 bool structure = false;
 QPixmap cls( ( const char** ) class_xpm );
 QPixmap sct( ( const char** ) struct_xpm );
 QPixmap mcr( ( const char** ) macro_xpm );
 Q3ListViewItem *node = NULL;
 Q3ListViewItem *mcrNode = NULL, *sctNode = NULL, *clsNode = NULL;
 Q3ListViewItem *lastMcrNode = NULL, *lastSctNode = NULL, *lastClsNode = NULL;

 KTextEditor::Document *kv = win->activeView()->document();

 //kDebug(13000)<<"Lines counted :"<<kv->numLines()<<endl;
 if(listMode)
   {
    mcrNode = new Q3ListViewItem(symbols, symbols->lastItem(), i18n("Macros"));
    sctNode = new Q3ListViewItem(symbols, symbols->lastItem(), i18n("Structures"));
    clsNode = new Q3ListViewItem(symbols, symbols->lastItem(), i18n("Functions"));
    mcrNode->setPixmap(0, (const QPixmap &)mcr);
    sctNode->setPixmap(0, (const QPixmap &)sct);
    clsNode->setPixmap(0, (const QPixmap &)cls);
    lastMcrNode = mcrNode;
    lastSctNode = sctNode;
    lastClsNode = clsNode;
    symbols->setRootIsDecorated(1);
   }
 else symbols->setRootIsDecorated(0);

 for (i=0; i<kv->lines(); i++)
   {
    cl = kv->line(i);
    cl = cl.trimmed();
    if ( (cl.length()>=2) && (cl.at(0) == '/' && cl.at(1) == '/')) continue;
    if(cl.indexOf("/*") >= 0 && graph == 0) comment = 1;
    if(cl.indexOf("*/") >= 0 && graph == 0) comment = 0;
    if(cl.indexOf('#') >= 0 && graph == 0 ) macro = 1;
    if (comment != 1)
      {
       /* *********************** MACRO PARSING *****************************/
       if(macro == 1)
         {
          //macro_pos = cl.indexOf('#');
          for (j = 0; j < cl.length(); j++)
             {
              if ( ((j+1) <cl.length()) &&  (cl.at(j)=='/' && cl.at(j+1)=='/')) { macro = 0; break; }
              if(  cl.indexOf("define") == j &&
                 !(cl.indexOf("defined") == j))
                    {
                     macro = 2;
                     j += 6; // skip the word "define"
                    }
              if(macro == 2 && cl.at(j) != ' ') macro = 3;
              if(macro == 3)
                {
                 if (cl.at(j) >= 0x20) stripped += cl.at(j);
                 if (cl.at(j) == ' ' || j == cl.length() - 1)
                         macro = 4;
                }
              //kDebug(13000)<<"Macro -- Stripped : "<<stripped<<" macro = "<<macro<<endl;
             }
           // I didn't find a valid macro e.g. include
           if(j == cl.length() && macro == 1) macro = 0;
           if(macro == 4)
             {
              //stripped.replace(0x9, " ");
              stripped = stripped.trimmed();
              if (macro_on == true)
                 {
                  if (listMode)
                    {
                     node = new Q3ListViewItem(mcrNode, lastMcrNode, stripped);
                     lastMcrNode = node;
                    }
                  else node = new Q3ListViewItem(symbols, symbols->lastItem(), stripped);
                  node->setPixmap(0, (const QPixmap &)mcr);
                  node->setText(1, QString::number( i, 10));
                 }
              macro = 0;
              macro_pos = 0;
              stripped = "";
              //kDebug(13000)<<"Macro -- Inserted : "<<stripped<<" at row : "<<i<<endl;
              if (cl.at(cl.length() - 1) == '\\') macro = 5; // continue in rows below
              continue;
             }
          }
       if (macro == 5 && cl.at(cl.length() - 1) != '\\') { macro = 0; continue; }

       /* ******************************************************************** */
       //if (cl.at(j) == '"' && comment == 2) { comment = 0; j++; }
       //else if (cl.at(j) == '"' && comment == 0) comment = 2;

       if(cl.indexOf('(') >= 0 && cl.at(0) != '#' && block == 0 && comment != 2)
          { structure = false; block = 1; }
       if((cl.indexOf("typedef") >= 0 || cl.indexOf("struct") >= 0) &&
          graph == 0 && block == 0)
         { structure = true; block = 2; stripped = ""; }
       //if(cl.indexOf(';') >= 0 && graph == 0)
       //    block = 0;
       if(block > 0)
         {
          for (j = 0; j < cl.length(); j++)
            {
             if (cl.at(j) == '/' && (cl.at(j + 1) == '*')) comment = 2;
             if ( ((j+1)<cl.length()) && (cl.at(j) == '*' && (cl.at(j + 1) == '/')) ) {  comment = 0; j+=2; if (j>=cl.length()) break;}
             if (cl.at(j) == '"' && comment == 2) { comment = 0; j++; if (j>=cl.length()) break;}
             else if (cl.at(j) == '"' && comment == 0) comment = 2;
             if ( ((j+1) <cl.length()) &&(cl.at(j)=='/' && cl.at(j+1)=='/') )
               { if(block == 1 && stripped.isEmpty()) block = 0; break; }
             if (comment != 2)
               {
                if (block == 1 && graph == 0 )
                  {
                   if(cl.at(j) >= 0x20) stripped += cl.at(j);
                   if(cl.at(j) == '(') par++;
                   if(cl.at(j) == ')')
                     {
                      par--;
                      if(par == 0)
                        {
                         stripped = stripped.trimmed();
                         stripped.remove("static ");
                         //kDebug(13000)<<"Function -- Inserted : "<<stripped<<" at row : "<<i<<endl;
                         block = 2;
                         tmpPos = i;
                        }
                     }
                  } // BLOCK 1
                if(block == 2 && graph == 0)
                  {
                   if ( ((j+1)<cl.length()) && (cl.at(j)=='/' && cl.at(j+1)=='/') ) break;
                   //if(cl.at(j)==':' || cl.at(j)==',') { block = 1; continue; }
                   if(cl.at(j)==':') { block = 1; continue; }
                   if(cl.at(j)==';')
                     {
                      stripped = "";
                      block = 0;
                      structure = false;
                      break;
                     }

                   if(cl.at(j)=='{' && structure == false && cl.indexOf(';') < 0)
                     {
                      stripped.replace(0x9, " ");
                      if(func_on == true)
                        {
                         if (types_on == false)
                           {
                            while (stripped.indexOf('(') >= 0)
                              stripped = stripped.left(stripped.indexOf('('));
                            while (stripped.indexOf("::") >= 0)
                              stripped = stripped.mid(stripped.indexOf("::") + 2);
                            stripped = stripped.trimmed();
                            while (stripped.indexOf(0x20) >= 0)
                              stripped = stripped.mid(stripped.indexOf(0x20, 0) + 1);
                           }
                         if (listMode)
                           {
                            node = new Q3ListViewItem(clsNode, lastClsNode, stripped);
                            lastClsNode = node;
                           }
                         else node = new Q3ListViewItem(symbols, symbols->lastItem(), stripped);
                         node->setPixmap(0, (const QPixmap &)cls);
                         node->setText(1, QString::number( tmpPos, 10));
                        }
                      stripped = "";
                      retry = 0;
                      block = 3;
                     }
                   if(cl.at(j)=='{' && structure == true)
                     {
                      block = 3;
                      tmpPos = i;
                     }
                   if(cl.at(j)=='(' && structure == true)
                     {
                      retry = 1;
                      block = 0;
                      j = 0;
                      //kDebug(13000)<<"Restart from the beginning of line..."<<endl;
                      stripped = "";
                      break; // Avoid an infinite loop :(
                     }
                   if(structure == true && cl.at(j) >= 0x20) stripped += cl.at(j);
                  } // BLOCK 2

                if (block == 3)
                  {
                   // A comment...there can be anything
                   if( ((j+1)<cl.length()) && (cl.at(j)=='/' && cl.at(j+1)=='/') ) break;
                   if(cl.at(j)=='{') graph++;
                   if(cl.at(j)=='}')
                     {
                      graph--;
                      if (graph == 0 && structure == false) block = 0;
                      if (graph == 0 && structure == true) block = 4;
                     }
                  } // BLOCK 3

                if (block == 4)
                  {
                   if(cl.at(j) == ';')
                     {
                      //stripped.replace(0x9, " ");
                      stripped.remove('{');
                      stripped.replace('}', " ");
                      if(struct_on == true)
                        {
                         if (listMode)
                           {
                            node = new Q3ListViewItem(sctNode, lastSctNode, stripped);
                            lastSctNode = node;
                           }
                         else node = new Q3ListViewItem(symbols, symbols->lastItem(), stripped);
                         node->setPixmap(0, (const QPixmap &)sct);
                         node->setText(1, QString::number( tmpPos, 10));
                        }
                      //kDebug(13000)<<"Structure -- Inserted : "<<stripped<<" at row : "<<i<<endl;
                      stripped = "";
                      block = 0;
                      structure = false;
                      //break;
                      continue;
                     }
                   if (cl.at(j) >= 0x20) stripped += cl.at(j);
                  } // BLOCK 4
               } // comment != 2
             //kDebug(13000)<<"Stripped : "<<stripped<<" at row : "<<i<<endl;
            } // End of For cycle
         } // BLOCK > 0
      } // Comment != 1
   } // for kv->numlines

 //for (i= 0; i < (symbols->itemIndex(node) + 1); i++)
 //    kDebug(13000)<<"Symbol row :"<<positions.at(i) <<endl;
}



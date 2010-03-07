/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2010 Nico Schl\"omer

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "GL_StatsWriter.h"

// ============================================================================
GL::StatsWriter::StatsWriter( std::string & fileName ):
    fileStream_(),
    statisticsList_( Teuchos::rcp( new Teuchos::ParameterList() ) ),
    printHeader_( true )
{
    // Set the output format
    // Think about replacing this with NOX::Utils::Sci.
    fileStream_.setf ( std::ios::scientific );
    fileStream_.precision ( 15 );
    
    std::cout << "eeee " << fileName << std::endl;

    fileStream_.open ( fileName.c_str(), std::ios::trunc );
    return;
}
// ============================================================================
GL::StatsWriter::~StatsWriter()
{
  fileStream_.close();
}
// ============================================================================
Teuchos::RCP<Teuchos::ParameterList>
GL::StatsWriter::
getList()
{
    return statisticsList_; 
}
// ============================================================================
void
GL::StatsWriter::
setList( const Teuchos::RCP<Teuchos::ParameterList> statisticsList )
{
    statisticsList_ = statisticsList;
    return;
}
// ============================================================================
void
GL::StatsWriter::
print()
{ 
   int doublePrec = 15;
   int columnWidth = doublePrec + 7;
   
   unsigned int intColumnWidth = 5;
   
   std::string columnSep = "  ";
   std::string headerStart = "# ";
   
   Teuchos::ParameterList::ConstIterator k;
   
   if ( printHeader_ )
   {
       // start of the header line
       fileStream_ << headerStart;
       for ( k=statisticsList_->begin(); k!=statisticsList_->end(); ++k )
       {
         std::stringstream strstream;
         strstream.fill( ' ' );
         strstream << std::left; // have the number flush left
         
         std::string label = statisticsList_->name(k);
         if ( statisticsList_->isType<double>( label ) )
             strstream.width( columnWidth );
         else
             strstream.width( (intColumnWidth<label.length())?
                              label.length():intColumnWidth
                            );
           
         strstream << statisticsList_->name(k);
         fileStream_ << strstream.str() << columnSep;
       }
       // flush:
       fileStream_ << std::endl;
       printHeader_ = false; 
   }


   // pad from the left with the width of the header line starter
   // to get column alignment
   fileStream_ << std::string( headerStart.length(), ' ' );

   for ( k=statisticsList_->begin(); k!=statisticsList_->end(); ++k )
   {
       std::stringstream strstream;
       strstream.fill( ' ' );
       strstream << std::left; // have the number flush left
       
     
       std::string label = statisticsList_->name(k);
    
       if ( statisticsList_->isType<double>( label ) )
       {
           strstream.width( columnWidth );
           strstream.setf ( std::ios::scientific );
           strstream.precision( doublePrec );
           strstream << statisticsList_->get<double>( label );
       }
       else if ( statisticsList_->isType<int>( label ) )
       {
           strstream.width( (intColumnWidth<label.length())?
                            label.length():intColumnWidth
                          );
           strstream << statisticsList_->get<int>( label );
       }

       fileStream_ << strstream.str() << columnSep;
  }
  
  // flush:
  fileStream_ << std::endl;

  return;
}
// ============================================================================
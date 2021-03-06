/*
    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) 2010  Nico Sch\"omer

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

#ifndef VIO_IMAGE_READER_FACTORY_H
#define VIO_IMAGE_READER_FACTORY_H

#include "VIO_Image_Reader_Abstract.h"

namespace VIO
{
  namespace Image {
  namespace Reader
  {
  
class Factory
{
public:
    //! Returns an pointer to an object of type IoVirtual for a given file
    //! fileName.
    //! Which of the implementations of IoVirtual is chosen is determined
    //! according to the suffix if fileName.
    static Teuchos::RCP<VIO::Image::Reader::Abstract>
    create ( const std::string & fileName );

protected:
private:
};

  } // namespace Reader
 }
} // namespace reader

#endif // VIO_IMAGE_READER_FACTORY_H

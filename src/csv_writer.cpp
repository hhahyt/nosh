#include "csv_writer.hpp"

#include <string>

namespace nosh
{
// ============================================================================
csv_writer::
csv_writer(const std::string &file_name,
           const std::string &delimeter
          ):
  fileStream_(),
  delimeter_(delimeter),
  headerStart_("#"),
  doublePrec_(15),
  doubleColumnWidth_(doublePrec_ + 7),
  intColumnWidth_(5)
{
  if (!file_name.empty()) {
    // Set the output format.
    // Think about replacing this with NOX::Utils::Sci.
    fileStream_.setf(std::ios::scientific);
    fileStream_.precision(15);

    fileStream_.open(file_name.c_str(), std::ios::trunc);
  }
  return;
}
// ============================================================================
csv_writer::
~csv_writer()
{
  if (fileStream_.is_open())
    fileStream_.close();
}
// ============================================================================
void
csv_writer::
write_header(const Teuchos::ParameterList & pList) const
{
  if (fileStream_.is_open()) {
    fileStream_ << headerStart_ << " ";

    bool isFirst = true;
    for (Teuchos::ParameterList::ConstIterator k = pList.begin();
         k != pList.end();
         ++k) {
      if (!isFirst)
        fileStream_ << delimeter_ << "  ";
      isFirst = false;

      std::stringstream strstream;
      strstream.fill(' ');
      strstream << std::left;

      std::string label = pList.name(k);
      if (pList.isType<double>(label))
        strstream.width(doubleColumnWidth_);
      else if (pList.isType<int>(label)
                || pList.isType<unsigned int>(label))
        strstream.width((intColumnWidth_ < label.length()) ?
                         label.length() : intColumnWidth_
                       );
      else if (pList.isType<std::string>(label))
        strstream.width(pList.get<std::string>(label).length());
      else
        TEUCHOS_TEST_FOR_EXCEPT_MSG(true,
                                    "Invalid data type for item \""
                                    << label << "\".");

      strstream << pList.name(k);

      // Write it out to the file.
      fileStream_ << strstream.str();
    }
    // flush:
    fileStream_ << std::endl;
  }

  return;
}
// ============================================================================
void
csv_writer::
write_row(const Teuchos::ParameterList & pList) const
{
  if (fileStream_.is_open()) {
    // Pad from the left with the width of the header line starter
    // to get column alignment.
    fileStream_ << std::string(headerStart_.length() + 1, ' ');

    bool isFirst = true;
    for (Teuchos::ParameterList::ConstIterator k = pList.begin();
         k != pList.end();
         ++k) {
      if (!isFirst)
        fileStream_ << delimeter_ << "  ";
      isFirst = false;

      std::stringstream strstream;
      strstream.fill(' ');
      strstream << std::left;    // have the number flush left

      std::string label = pList.name(k);
      if (pList.isType<double>(label)) {
        strstream.width(doubleColumnWidth_);
        strstream.setf(std::ios::scientific);
        strstream.precision(doublePrec_);
        strstream << pList.get<double>(label);
      } else if (pList.isType<int>(label)) {
        strstream.width((intColumnWidth_ < label.length()) ?
                         label.length() : intColumnWidth_
                      );
        strstream << pList.get<int>(label);
      } else if (pList.isType<unsigned int>(label)) {
        strstream.width((intColumnWidth_ < label.length()) ?
                         label.length() : intColumnWidth_
                      );
        strstream << pList.get<unsigned int>(label);
      } else if (pList.isType<std::string>(label)) {
        strstream.width(pList.get<std::string>(label).length());
        strstream << pList.get<std::string>(label);
      } else {
        TEUCHOS_TEST_FOR_EXCEPT_MSG(
            true,
            "Invalid data type for item \"" << label << "\"."
            );
      }

      // Write it out to the file.
      fileStream_ << strstream.str();
    }

    // flush:
    fileStream_ << std::endl;
  }

  return;
}
// ============================================================================
}  // namespace nosh

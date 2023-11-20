// -----------------------------------------------------------------------------------------------------------
// Phext Encoding Definitions Header
// (c) 2023 Will Bickford
// MIT License
// -----------------------------------------------------------------------------------------------------------

#pragma once

namespace phext
{
   static constexpr size_t SUMMARY_LINE_LENGTH = 64;
   enum Break : uint8_t
   {
      STRING     = 0x00, // 1st
      LINE       = 0x0A, // 2nd (Newline)
      SCROLL     = 0x17, // 3rd
      SECTION    = 0x18, // 4th
      CHAPTER    = 0x19, // 5th
      BOOK       = 0x1A, // 6th
      VOLUME     = 0x1C, // 7th
      COLLECTION = 0x1D, // 8th
      SERIES     = 0x1E, // 9th
      SHELF      = 0x1F, // 10th
      LIBRARY    = 0x01  // 11th
   };
   static constexpr uint8_t SCROLL_BREAK     = Break::SCROLL;
   static constexpr uint8_t SECTION_BREAK    = Break::SECTION;
   static constexpr uint8_t CHAPTER_BREAK    = Break::CHAPTER;
   static constexpr uint8_t BOOK_BREAK       = Break::BOOK;
   static constexpr uint8_t VOLUME_BREAK     = Break::VOLUME;
   static constexpr uint8_t COLLECTION_BREAK = Break::COLLECTION;
   static constexpr uint8_t SERIES_BREAK     = Break::SERIES;
   static constexpr uint8_t SHELF_BREAK      = Break::SHELF;
   static constexpr uint8_t LIBRARY_BREAK    = Break::LIBRARY;

   struct Coordinate
   {
		static constexpr uint16_t LIMIT = 1024; // cap dimension size at 1K for now
      uint16_t LibraryID{1};
      uint16_t ShelfID{1};
      uint16_t SeriesID{1};
      uint16_t CollectionID{1};
      uint16_t VolumeID{1};
      uint16_t BookID{1};
      uint16_t ChapterID{1};
      uint16_t SectionID{1};
      uint16_t ScrollID{1};

      void libraryBreak()
      {
         LibraryID += 1;
         ShelfID = 1;
         SeriesID = 1;
         CollectionID = 1;
         VolumeID = 1;
         BookID = 1;
         ChapterID = 1;
         SectionID = 1;
         ScrollID = 1;
      }

      void shelfBreak()
      {
         ShelfID += 1;
         SeriesID = 1;
         CollectionID = 1;
         VolumeID = 1;
         BookID = 1;
         ChapterID = 1;
         SectionID = 1;
         ScrollID = 1;
      }

      void seriesBreak()
      {
         SeriesID += 1;
         CollectionID = 1;
         VolumeID = 1;
         BookID = 1;
         ChapterID = 1;
         SectionID = 1;
         ScrollID = 1;
      }

      void collectionBreak()
      {
         CollectionID += 1;
         VolumeID = 1;
         BookID = 1;
         ChapterID = 1;
         SectionID = 1;
         ScrollID = 1;
      }

      void volumeBreak()
      {
         VolumeID += 1;
         BookID = 1;
         ChapterID = 1;
         SectionID = 1;
         ScrollID = 1;
      }

      void bookBreak()
      {
         BookID += 1;
         ChapterID = 1;
         SectionID = 1;
         ScrollID = 1;
      }

      void chapterBreak()
      {
         ChapterID += 1;
         SectionID = 1;
         ScrollID = 1;
      }

      void sectionBreak()
      {
         SectionID += 1;
         ScrollID = 1;
      }

      void scrollBreak()
      {
         ScrollID += 1;
      }
   };
}
SECTIONS
{
       __buildid_note_section = . ;
       .note.gnu.build-id :
       {
               *(.note.gnu.build-id)
       }
       __buildid_end = . ;
}
INSERT AFTER .data

//
// Struct ADT
// By Martin Nilsson
// $Id: Struct.pike,v 1.6 2003/07/04 19:26:39 nilsson Exp $
//

//! Implements a struct which can be used for serialization and
//! deserialization of data.
//! @example
//!   class ID3 {
//!     inherit ADT.Struct;
//!     Item head = Chars(3);
//!     Item title = Chars(30);
//!     Item artist = Chars(30);
//!     Item album = Chars(30);
//!     Item year = Chars(4);
//!     Item genre = Byte();
//!     Item comment = Chars(30);
//!   }
//!
//!   Stdio.File f = Stdio.File("foo.mp3");
//!   f->seek(-128);
//!   ADT.Struct tag = ID3(f);
//!   if(tag->head=="TAG") {
//!     write("Title: %s\n", tag->title);
//!     tag->title = "A new title" + "\0"*19;
//!     f->seek(-128);
//!     f->write( (string)tag );
//!   }

static int item_counter;
static array(Item) items = ({});
static mapping(string:Item) names = ([]);
static int(0..1) booted;

//! @decl void create(void|string|Stdio.File data)
//! @param data
//!   Data to be decoded and populate the struct. Can
//!   either be a file object or a string.
void create(void|string|object file) {
  foreach(indices(this_object()), string index) {
    mixed val = ::`[](index, 2);
    if(objectp(val) && val->is_item) names[index]=val;
  }
  booted = 1;
  items = values(names);
  sort(items->id, items);

  if(file) decode(file);
}

//! @decl void decode(string|Stdio.File data)
//! Decodes @[data] according to the struct and populates
//! the struct variables. The @[data] can either be a file
//! object or a string.
void decode(string|object file) {
  if(stringp(file)) file = Stdio.FakeFile(file);
  items->decode(file);
}

//! Serializes the struct into a string. This string is equal
//! to the string fed to @[decode] if nothing in the struct
//! has been altered.
string encode() {
  return items->encode()*"";
}


// --- LFUN overloading.

//! @decl static mixed `[](string item)
//! @decl static mixed `->(string item)
//! The struct can be indexed by item name to get the
//! associated value.

//! @decl static mixed `[]=(string item)
//! @decl static mixed `->=(string item)
//! It is possible to assign a new value to a struct
//! item by indexing it by name and assign a value.

static mixed `[](string id) {
  if(names[id]) return names[id]->get();
  return ::`[](id, 2);
}

static mixed `[]=(string id, mixed value) {
  if(names[id]) names[id]->set(value);
  return id;
}

static function `-> = `[];
static function `->= = `[]=;

//! The indices of a struct is the name of the struct items.
static array(string) _indices() {
  if(!booted) return ::_indices(2);
  array ret = indices(names);
  sort(values(names)->id, ret);
  return ret;
}

//! The values of a struct is the values of the struct items.
static array _values() {
  return items->get();
}

//! The size of the struct object is the number of bytes
//! allocated for the struct.
static int _sizeof() {
  return `+( 0, @items->size );
}

//! The struct can be casted into a string, which is eqivivalent
//! to running @[encode], or into an array. When casted into an
//! array each array element is the encoded value of that struct
//! item.
static mixed cast(string to) {
  switch(to) {
  case "string": return encode();
  case "array": return items->encode();
  }
}


// --- Virtual struct item class

//! Interface class for struct items.
class Item {
  int id = item_counter++;
  constant is_item=1;

  static mixed value;
  int size;

  //! @ignore
  void decode(object);
  string encode();
  //! @endignore

  void set(mixed in) { value=in; }
  mixed get() { return value; }

  static string _sprintf(int t) {
    return t=='O' && sprintf("%O(%O)", this_program, value);
  }
}


// --- Struct items.

//! One byte, integer value between 0 and 255.
class Byte {
  inherit Item;
  int size = 1;
  static int(0..255) value;

  //! The byte can be initialized with an optional value.
  void create(void|int(0..255) initial_value) {
    set(initial_value);
  }

  void set(int(0..255) in) {
    if(in<0 || in>255) error("Value out of bound\n");
    value = in;
  }
  void decode(object f) { sscanf(f->read(1), "%c", value); }
  string encode() { return sprintf("%c", value); }

  static string _sprintf(int t) {
    return t=='O' && sprintf("%O(%d/%O)", this_program, value, (string)({value}));
  }
}

//! One word in network order, integer value between 0 and 65535.
//! @seealso
//! @[Drow]
class Word {
  inherit Item;
  int size = 2;
  static int(0..) value;

  //! The byte can be initialized with an optional value.
  void create(void|int(0..) initial_value) {
    set(initial_value);
  }

  void set(int(0..) in) {
    if(in<0 || in>=pow(2,size*8)) error("Value out of bound.\n");
    value = in;
  }
  void decode(object f) { sscanf(f->read(size), "%"+size+"c", value); }
  string encode() { return sprintf("%"+size+"c", value); }

  static string _sprintf(int t) {
    return t=='O' && sprintf("%O(%d)", this_program, value);
  }
}

//! One word in intel order, integer value between 0 and 65535.
//! @seealso
//! @[Word]
class Drow {
  inherit Word;
  void decode(object f) { sscanf(f->read(size), "%-"+size+"c", value); }
  string encode() { return sprintf("%-"+size+"c", value); }
}

//! One longword in network order, integer value between 0 and 2^32.
//! @seealso
//! @[Gnol]
class Long {
  inherit Word;
  int size = 4;
}

//! One longword in inte order, integer value between 0 and 2^32.
//! @seealso
//! @[Long]
class Gnol {
  inherit Drow;
  int size = 4;
}

//! A string of bytes.
class Chars {
  inherit Item;
  int size;
  static string value;

  //! @decl static void create(int size, void|string value)
  //! The number of bytes that are part of this struct item.
  //! The initial value of the char string is @[value] or,
  //! if not provided, a string of zero bytes.
  static void create(int _size, void|string _value) {
    size = _size;
    if(_value)
      set(_value);
    else
      value = "\0"*size;
  }

  void set(string in) {
    if(sizeof(in)!=size) error("String has wrong size.\n");
    if(String.width(in)!=8) error("Wide strings not allowed.\n");
    value = in;
  }
  void decode(object f) { value=f->read(size); }
  string encode() { return value; }

  static string _sprintf(int t) {
    return t=='O' && sprintf("%O(%O)", this_program, value);
  }
}

string _sprintf(int t) {
  return t=='O' && sprintf("%O(%O)", this_program, items);
}

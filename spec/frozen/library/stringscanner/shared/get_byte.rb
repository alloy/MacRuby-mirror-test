# encoding: UTF-8
require File.expand_path('../eucjp', __FILE__)

describe :strscan_get_byte, :shared => true do
  it "scans one byte and returns it" do
    s = StringScanner.new('abc5.')
    s.send(@method).should == 'a'
    s.send(@method).should == 'b'
    s.send(@method).should == 'c'
    s.send(@method).should == '5'
    s.send(@method).should == '.'
  end

  it "is not multi-byte character sensitive" do
    s = StringScanner.new("あ") 
    s.send(@method).should == "\xE3"
    s.send(@method).should == "\x81"
    s.send(@method).should == "\x82"
    s.send(@method).should == nil
  end
  
  it "is not multi-byte character sensitive and can handle encodings" do
    s = StringScanner.new(TestStrings.eucjp)
    s.send(@method).should == TestStrings.first_char
    s.send(@method).should == TestStrings.second_char
    s.send(@method).should == nil
  end 

  it "returns nil at the end of the string" do
    # empty string case
    s = StringScanner.new('')
    s.send(@method).should == nil
    s.send(@method).should == nil

    # non-empty string case
    s = StringScanner.new('a')
    s.send(@method) # skip one
    s.send(@method).should == nil
  end
end

/*
 * MacRuby ObjC helpers.
 *
 * This file is covered by the Ruby license. See COPYING for more details.
 * 
 * Copyright (C) 2007-2010, Apple Inc. All rights reserved.
 */

#include <Foundation/Foundation.h>
#include "ruby/macruby.h"
#include "ruby/node.h"
#include "ruby/encoding.h"
#include "ruby/objc.h"
#include "vm.h"
#include "objc.h"
#include "id.h"
#include "class.h"

#include <unistd.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <sys/mman.h>
#if HAVE_BRIDGESUPPORT_FRAMEWORK
# include <BridgeSupport/BridgeSupport.h>
#else
# include "bs.h"
#endif

bool
rb_objc_get_types(VALUE recv, Class klass, SEL sel, Method method,
		  bs_element_method_t *bs_method, char *buf, size_t buflen)
{
    const char *type;
    unsigned i;

    memset(buf, 0, buflen);

    if (method != NULL) {
	if (bs_method == NULL) {
	    type = method_getTypeEncoding(method);
	    assert(strlen(type) < buflen);
	    buf[0] = '\0';
	    do {
		const char *type2 = SkipFirstType(type);
		strncat(buf, type, type2 - type);
		type = SkipStackSize(type2);
	    }
	    while (*type != '\0');
	}
	else {
	    char buf2[100];
	    type = rb_get_bs_method_type(bs_method, -1);
	    if (type != NULL) {
		strlcpy(buf, type, buflen);
	    }
	    else {
		method_getReturnType(method, buf2, sizeof buf2);
		strlcpy(buf, buf2, buflen);
	    }

	    const unsigned int argc = rb_method_getNumberOfArguments(method);
	    for (i = 0; i < argc; i++) {
		if (i >= 2 && (type = rb_get_bs_method_type(bs_method, i - 2))
			!= NULL) {
		    strlcat(buf, type, buflen);
		}
		else {
		    method_getArgumentType(method, i, buf2, sizeof(buf2));
		    strlcat(buf, buf2, buflen);
		}
	    }
	}
	return true;
    }
    else if (!SPECIAL_CONST_P(recv)) {
	NSMethodSignature *msig = [(id)recv methodSignatureForSelector:sel];
	if (msig != NULL) {
	    unsigned i;

	    type = rb_get_bs_method_type(bs_method, -1);
	    if (type == NULL) {
		type = [msig methodReturnType];
	    }
	    strlcpy(buf, type, buflen);

	    const int argc = [msig numberOfArguments];
	    for (i = 0; i < argc; i++) {
		if (i < 2 || (type = rb_get_bs_method_type(bs_method, i - 2))
			== NULL) {
		    type = [msig getArgumentTypeAtIndex:i];
		}
		strlcat(buf, type, buflen);
	    }

	    return true;
	}
    }
    return false;
}

bool
rb_objc_supports_forwarding(VALUE recv, SEL sel)
{
    if (!SPECIAL_CONST_P(recv)) {
	return [(id)recv methodSignatureForSelector:sel] != nil;
    }
    return false;
}

VALUE
rb_home_dir(VALUE user_name)
{
    NSString *user = nil;
    NSString *home_dir = nil;

    if (user_name != Qnil) {
	user = (NSString *)user_name;
	home_dir = NSHomeDirectoryForUser(user);
	if (home_dir == nil) {
	    rb_raise(rb_eArgError, "user %s doesn't exist",
		[user UTF8String]);
	}
    }
    else {
	home_dir = NSHomeDirectory();
	if (home_dir == nil) {
	    return Qnil;
	}
    }
    return rb_str_new2([home_dir fileSystemRepresentation]);
}

static BOOL
is_absolute_path(NSString *path, int expand_tilde)
{
    if (!expand_tilde && [path isAbsolutePath] && [path hasPrefix:@"~"]) {
	return NO;
    }
    return [path isAbsolutePath];    
}

static VALUE
file_expand_path(VALUE fname, VALUE dname, int absolute)
{
    NSString *res = (NSString *)FilePathValue(fname);

    if (is_absolute_path(res, !absolute)) {
	NSString *tmp = [res stringByResolvingSymlinksInPath];
	// Make sure we don't have an invalid user path.
	if ([res hasPrefix:@"~"] && [tmp isEqualToString:res]) {
	    NSString *user = [[[res pathComponents] objectAtIndex:0]
		substringFromIndex:1];
	    rb_raise(rb_eArgError, "user %s doesn't exist", [user UTF8String]);
	}
	res = tmp;
    }
    else {
	NSString *dir = dname != Qnil
	    ? (NSString *)FilePathValue(dname)
	    : [[NSFileManager defaultManager] currentDirectoryPath];

	if (!is_absolute_path(dir, !absolute)) {
	    dir = (NSString *)file_expand_path((VALUE)dir, Qnil, 0);
	}

	// stringByStandardizingPath does not expand "/." to "/".
	if ([res isEqualToString:@"."] && [dir isEqualToString:@"/"]) {
	    res = @"/";
	}
	else {
	    res = [[dir stringByAppendingPathComponent:res]
		stringByStandardizingPath];
	}
    }

    return rb_str_new2([res fileSystemRepresentation]);
}

VALUE
rb_file_expand_path(VALUE fname, VALUE dname)
{
    return file_expand_path(fname, dname, 0);
}

VALUE
rb_file_absolute_path(VALUE fname, VALUE dname)
{
    return file_expand_path(fname, dname, 1);
}

static VALUE
rb_objc_load_bs(VALUE recv, SEL sel, VALUE path)
{
#if MACRUBY_STATIC
    not_implemented_in_static(sel);
#else
    rb_vm_load_bridge_support(StringValuePtr(path), NULL, 0);
    return recv;
#endif
}

#if !defined(MACRUBY_STATIC)
static void
rb_objc_search_and_load_bridge_support(const char *framework_path)
{
    char path[PATH_MAX];

    if (bs_find_path(framework_path, path, sizeof path)) {
	rb_vm_load_bridge_support(path, framework_path,
                                    BS_PARSE_OPTIONS_LOAD_DYLIBS);
    }
}
#endif

static void
reload_class_constants(void)
{
    static int class_count = 0;

    const int count = objc_getClassList(NULL, 0);
    if (count == class_count) {
	return;
    }

    Class *buf = (Class *)malloc(sizeof(Class) * count);
    objc_getClassList(buf, count);

    for (int i = 0; i < count; i++) {
	Class k = buf[i];
	if (!RCLASS_RUBY(k)) {
	    const char *name = class_getName(k);
	    if (name[0] != '_') {
		ID name_id = rb_intern(name);
		if (!rb_const_defined(rb_cObject, name_id)) {
		    rb_const_set(rb_cObject, name_id, (VALUE)k);
		}
	    }
	}
    }

    class_count = count;

    free(buf);
}

bool rb_objc_enable_ivar_set_kvo_notifications = false;

VALUE
rb_require_framework(VALUE recv, SEL sel, int argc, VALUE *argv)
{
#if MACRUBY_STATIC
    not_implemented_in_static(sel);
#else
    VALUE framework;
    VALUE search_network;
    const char *cstr;
    NSFileManager *fileManager;
    NSString *path;
    NSBundle *bundle;
    NSError *error;
    
    rb_scan_args(argc, argv, "11", &framework, &search_network);

    Check_Type(framework, T_STRING);
    cstr = RSTRING_PTR(framework);

    fileManager = [NSFileManager defaultManager];
    path = [fileManager stringWithFileSystemRepresentation:cstr
	length:strlen(cstr)];

    if (![fileManager fileExistsAtPath:path]) {
	/* framework name is given */
	NSSearchPathDomainMask pathDomainMask;
	NSString *frameworkName;
	NSArray *dirs;
	NSUInteger i, count;

	cstr = NULL;

#define FIND_LOAD_PATH_IN_LIBRARY(dir) 					  \
    do { 								  \
	path = [[dir stringByAppendingPathComponent:@"Frameworks"]	  \
	   stringByAppendingPathComponent:frameworkName];		  \
	if ([fileManager fileExistsAtPath:path]) {			  \
	    goto success; 						  \
	}								  \
	path = [[dir stringByAppendingPathComponent:@"PrivateFrameworks"] \
	   stringByAppendingPathComponent:frameworkName];		  \
	if ([fileManager fileExistsAtPath:path]) {			  \
	    goto success; 						  \
	}								  \
    } 									  \
    while (0)

	pathDomainMask = RTEST(search_network)
	    ? NSAllDomainsMask
	    : NSUserDomainMask | NSLocalDomainMask | NSSystemDomainMask;

	frameworkName = [path stringByAppendingPathExtension:@"framework"];

	path = [[[[NSBundle mainBundle] bundlePath] 
	    stringByAppendingPathComponent:@"Contents/Frameworks"] 
		stringByAppendingPathComponent:frameworkName];
	if ([fileManager fileExistsAtPath:path]) {
	    goto success;
	}

	dirs = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, 
	    pathDomainMask, YES);
	for (i = 0, count = [dirs count]; i < count; i++) {
	    NSString *dir = [dirs objectAtIndex:i];
	    FIND_LOAD_PATH_IN_LIBRARY(dir);
	}	

	dirs = NSSearchPathForDirectoriesInDomains(NSDeveloperDirectory, 
	    pathDomainMask, YES);
	for (i = 0, count = [dirs count]; i < count; i++) {
	    NSString *dir = [[dirs objectAtIndex:i] 
		stringByAppendingPathComponent:@"Library"];
	    FIND_LOAD_PATH_IN_LIBRARY(dir); 
	}
	
    dirs = [[[[NSProcessInfo processInfo] environment] valueForKey:@"DYLD_FRAMEWORK_PATH"] componentsSeparatedByString: @":"];
    for (i = 0, count = [dirs count]; i < count; i++) {
        NSString *dir = [dirs objectAtIndex:i];
        path = [dir stringByAppendingPathComponent:frameworkName];
        if ([fileManager fileExistsAtPath:path]) {
            goto success;
	}
    }

#undef FIND_LOAD_PATH_IN_LIBRARY

	rb_raise(rb_eRuntimeError, "framework `%s' not found", 
		RSTRING_PTR(framework));
    }

success:

    if (cstr == NULL) {
	cstr = [path fileSystemRepresentation];
    }

    bundle = [NSBundle bundleWithPath:path];
    if (bundle == nil) {
	rb_raise(rb_eRuntimeError, 
	         "framework at path `%s' cannot be located",
		 cstr);
    }

    const bool loaded = [bundle isLoaded];
    if (!loaded && ![bundle loadAndReturnError:&error]) {
	rb_raise(rb_eRuntimeError,
		 "framework at path `%s' cannot be loaded: %s",
		 cstr,
		 [[error description] UTF8String]); 
    }

    rb_objc_search_and_load_bridge_support(cstr);
    reload_class_constants();

    rb_objc_enable_ivar_set_kvo_notifications = true;

    return loaded ? Qfalse : Qtrue;
#endif
}

void rb_vm_init_compiler(void);

static void
rb_objc_kvo_setter_imp(void *recv, SEL sel, void *value)
{
    const char *selname;
    char buf[128];
    size_t s;   

    selname = sel_getName(sel);
    buf[0] = '@';
    buf[1] = tolower(selname[3]);
    s = strlcpy(&buf[2], &selname[4], sizeof buf - 2);
    buf[s + 1] = '\0';

    rb_ivar_set((VALUE)recv, rb_intern(buf), value == NULL
	    ? Qnil : OC2RB(value));
}

/*
  Defines an attribute writer method which conforms to Key-Value Coding.
  (See http://developer.apple.com/documentation/Cocoa/Conceptual/KeyValueCoding/KeyValueCoding.html)
  
    attr_accessor :foo
  
  Will create the normal accessor methods, plus <tt>setFoo</tt>
  
  TODO: Does not handle the case were the user might override #foo=
*/
void
rb_objc_define_kvo_setter(VALUE klass, ID mid)
{
    char buf[100];
    const char *mid_name;

    buf[0] = 's'; buf[1] = 'e'; buf[2] = 't';
    mid_name = rb_id2name(mid);

    buf[3] = toupper(mid_name[0]);
    buf[4] = '\0';
    strlcat(buf, &mid_name[1], sizeof buf);
    strlcat(buf, ":", sizeof buf);

    if (!class_addMethod((Class)klass, sel_registerName(buf), 
			 (IMP)rb_objc_kvo_setter_imp, "v@:@")) {
	rb_warning("can't register `%s' as an KVO setter on class `%s' "\
		"(method `%s')",
		mid_name, rb_class2name(klass), buf);
    }
}

VALUE
rb_mod_objc_ib_outlet(VALUE recv, SEL sel, int argc, VALUE *argv)
{
    int i;

    rb_warn("ib_outlet has been deprecated, please use attr_writer instead");

    for (i = 0; i < argc; i++) {
	VALUE sym = argv[i];
	
	Check_Type(sym, T_SYMBOL);
	rb_objc_define_kvo_setter(recv, SYM2ID(sym));
    }

    return recv;
}

static void *__obj_flags; // used as a static key

long
rb_objc_flag_get_mask(const void *obj)
{
    return (long)rb_objc_get_associative_ref((void *)obj, &__obj_flags);
}

bool
rb_objc_flag_check(const void *obj, int flag)
{
    const long v = rb_objc_flag_get_mask(obj);
    if (v == 0) {
	return false; 
    }
    return (v & flag) == flag;
}

void
rb_objc_flag_set(const void *obj, int flag, bool val)
{
    long v = (long)rb_objc_get_associative_ref((void *)obj, &__obj_flags);
    if (val) {
	v |= flag;
    }
    else {
	v ^= flag;
    }
    rb_objc_set_associative_ref((void *)obj, &__obj_flags, (void *)v);
}

static IMP old_imp_isaForAutonotifying;

static Class
rb_obj_imp_isaForAutonotifying(void *rcv, SEL sel)
{
    long ret_version;

    Class ret = ((Class (*)(void *, SEL))old_imp_isaForAutonotifying)(rcv, sel);

    if (ret != NULL && ((ret_version = RCLASS_VERSION(ret)) & RCLASS_KVO_CHECK_DONE) == 0) {
	const char *name = class_getName(ret);
	if (strncmp(name, "NSKVONotifying_", 15) == 0) {
	    Class ret_orig;
	    name += 15;
	    ret_orig = objc_getClass(name);
	    if (ret_orig != NULL) {
		const long orig_v = RCLASS_VERSION(ret_orig);
		if ((orig_v & RCLASS_IS_OBJECT_SUBCLASS) == RCLASS_IS_OBJECT_SUBCLASS) {
		    ret_version |= RCLASS_IS_OBJECT_SUBCLASS;
		}
		if ((orig_v & RCLASS_IS_RUBY_CLASS) == RCLASS_IS_RUBY_CLASS) {
		    ret_version |= RCLASS_IS_RUBY_CLASS;
		}
	    }
	}
	ret_version |= RCLASS_KVO_CHECK_DONE;
	RCLASS_SET_VERSION(ret, ret_version);
    }
    return ret;
}

id
rb_rb2oc_exception(VALUE exc)
{
    NSString *name = [NSString stringWithUTF8String:rb_obj_classname(exc)];
    NSString *reason = (NSString *)rb_format_exception_message(exc);
    NSDictionary *dict = [NSDictionary dictionaryWithObject:(id)exc
	forKey:@"RubyException"];
    return [NSException exceptionWithName:name reason:reason userInfo:dict];
}

VALUE
rb_oc2rb_exception(id exc, bool *created)
{
    VALUE e;
    id rubyExc;

    rubyExc = [[exc userInfo] objectForKey:@"RubyException"];
    if (rubyExc == nil) {
	*created = true;

	char buf[1000];
	snprintf(buf, sizeof buf, "%s: %s", [[exc name] UTF8String],
		[[exc reason] UTF8String]);
	e = rb_exc_new2(rb_eRuntimeError, buf);
	// Set the backtrace for Obj-C exceptions
	rb_iv_set(e, "bt", rb_vm_backtrace(0));
    }
    else {
	*created = false;
	e = (VALUE)rubyExc;
    }
    return e;
}

void
rb_objc_exception_raise(const char *name, const char *message)
{
    assert(name != NULL && message != NULL);
    [[NSException exceptionWithName:[NSString stringWithUTF8String:name]
	reason:[NSString stringWithUTF8String:message] userInfo:nil] raise];
}

id
rb_objc_numeric2nsnumber(VALUE obj)
{
    if (FIXNUM_P(obj)) {
	// TODO: this could be optimized in case we can fit the fixnum
	// into an immediate NSNumber directly.
	long val = FIX2LONG(obj);
	CFNumberRef number = CFNumberCreate(NULL, kCFNumberLongType, &val);
	CFMakeCollectable(number);
	return (id)number;
    }
    if (FIXFLOAT_P(obj)) {
	double val = NUM2DBL(obj);
	CFNumberRef number = CFNumberCreate(NULL, kCFNumberDoubleType,
		&val);
	CFMakeCollectable(number);
	return (id)number;
    }
    abort();
}

static SEL sel_at = 0;

VALUE
rb_objc_convert_immediate(id obj)
{
    const bool is_immediate = ((unsigned long)obj & 0x1) == 0x1;

    Class orig_k = object_getClass(obj); // might be an immediate
    Class k = orig_k;
    do {
	if (k == (Class)rb_cNSNumber) {
	    // TODO: this could be optimized in case the object is an immediate.
	    if (CFNumberIsFloatType((CFNumberRef)obj)) {
		double v = 0;
		assert(CFNumberGetValue((CFNumberRef)obj, kCFNumberDoubleType, &v));
		return DOUBLE2NUM(v);
	    }
	    else {
		long v = 0;
		assert(CFNumberGetValue((CFNumberRef)obj, kCFNumberLongType, &v));
		return LONG2FIX(v);
	    }
	}
	else if (k == (Class)rb_cNSDate) {
	    @try {
		CFAbsoluteTime time = CFDateGetAbsoluteTime((CFDateRef)obj);
		VALUE arg = DBL2NUM(time + CF_REFERENCE_DATE);
		return rb_vm_call(rb_cTime, sel_at, 1, &arg);
	    }
	    @catch (NSException *e) {
		// Some NSDates might return an exception (example: uninitialized objects).
		break;
	    }
	}
	k = class_getSuperclass(k);
    }
    while (k != NULL);

    if (is_immediate) {
	rb_bug("unknown Objective-C immediate: %p (%s)\n", obj, class_getName(orig_k));
    }
    return (VALUE)obj;
}

bool
rb_objc_ignored_sel(SEL sel)
{
    return sel == @selector(retain)
	|| sel == @selector(release)
	|| sel == @selector(autorelease)
	|| sel == @selector(retainCount)
	|| sel == @selector(dealloc);
}

bool
rb_objc_isEqual(VALUE x, VALUE y)
{
    return [RB2OC(x) isEqual:RB2OC(y)];
}

void
rb_objc_force_class_initialize(Class klass)
{
    // Do not do anything in case the class is not NSObject-based
    // (likely Proxy).
    bool ok = false;
    for (Class k = klass; k != NULL; k = class_getSuperclass(k)) {
	if (k == (Class)rb_cNSObject) {
	    ok = true;
	    break;
	}
    }
    if (!ok) {
	return;
    }

    // This forces +initialize to be called.
    class_getMethodImplementation(klass, @selector(initialize));
}

size_t
rb_objc_type_size(const char *type)
{
    @try {
	NSUInteger size, align;
	NSGetSizeAndAlignment(type, &size, &align);
	return size;
    }
    @catch (id ex) {
	rb_raise(rb_eRuntimeError, "can't get the size of type `%s': %s",
		type, [[ex description] UTF8String]);
    }
    return 0; // never reached
}

static NSString *
relocated_load_path(NSString *path, NSString *macruby_path)
{
    NSRange r = [path rangeOfString:@"MacRuby.framework"];
    if (r.location == NSNotFound) {
	return nil;
    }
    r = NSMakeRange(0, r.location + r.length);
    return [path stringByReplacingCharactersInRange:r withString:macruby_path];
}

void
rb_objc_fix_relocatable_load_path(void)
{
    NSString *path = [[NSBundle mainBundle] privateFrameworksPath];
    path = [path stringByAppendingPathComponent:@"MacRuby.framework"];

    NSFileManager *fm = [NSFileManager defaultManager];
    if ([fm fileExistsAtPath:path]) {
	VALUE ary = rb_vm_load_path();
	for (long i = 0, count = RARRAY_LEN(ary); i < count; i++) {
	    NSString *p1 = (NSString *)RARRAY_AT(ary, i);
	    NSString *p2 = relocated_load_path(p1, path);
	    if (p2 != nil) {
		rb_ary_store(ary, i, (VALUE)p2);
	    }
	}
    }
}

void
rb_objc_willChangeValueForKey(id obj, NSString *key)
{
    [obj willChangeValueForKey:key];
}

void
rb_objc_didChangeValueForKey(id obj, NSString *key)
{
    [obj didChangeValueForKey:key];
}

static VALUE
rb_objc_load_plist(VALUE recv, SEL sel, VALUE str)
{
    StringValue(str);
    VALUE bstr = rb_str_bstr(str);
    NSData *data = [NSData dataWithBytes:rb_bstr_bytes(bstr)
	length:rb_bstr_length(bstr)];
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1060
    NSString *errorString = nil;
    id plist = [NSPropertyListSerialization propertyListFromData:data mutabilityOption:0
	format:NULL errorDescription:&errorString];
#else
    NSError *err = nil;
    id plist = [NSPropertyListSerialization propertyListWithData:data options:0
	format:NULL error:&err];
#endif
    if (plist == nil) {
	rb_raise(rb_eArgError, "error loading property list: '%s'",
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1060
		[errorString UTF8String]);
#else
		[[err localizedDescription] UTF8String]);
#endif
    }
    return OC2RB(plist);
}

extern VALUE rb_cNSDate;

static VALUE
rb_objc_to_plist(VALUE recv, SEL sel)
{
    switch (TYPE(recv)) {
	case T_STRING:
	case T_FIXNUM:
	case T_FLOAT:
	case T_BIGNUM:
	case T_HASH:
	case T_ARRAY:
	case T_TRUE:
	case T_FALSE:
	    break;

	default:
	    if (!rb_obj_is_kind_of(recv, rb_cNSDate)) {
		rb_raise(rb_eArgError,
			"object of class '%s' cannot be serialized to " \
			"property-list format",
			rb_obj_classname(recv));
	    }
    }

    id objc_obj = RB2OC(recv);
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1060
    NSString *errorString = nil;
    NSData *data = [NSPropertyListSerialization dataFromPropertyList:objc_obj
	format:NSPropertyListXMLFormat_v1_0 errorDescription:&errorString];
#else
    NSError *err = nil;
    NSData *data = [NSPropertyListSerialization dataWithPropertyList:objc_obj
	format:NSPropertyListXMLFormat_v1_0 options:0 error:&err];
#endif
    if (data == nil) {
	rb_raise(rb_eArgError, "error serializing property list: '%s'",
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1060
		[errorString UTF8String]);
#else
		[[err localizedDescription] UTF8String]);
#endif
    }
    const uint8_t* bytes = [data bytes];
    const size_t len = [data length];
    return rb_bstr_new_with_data(bytes, len);
}

void
Init_ObjC(void)
{
    sel_at = sel_registerName("at:");

    rb_objc_define_module_function(rb_mKernel, "load_bridge_support_file", rb_objc_load_bs, 1);
    rb_objc_define_module_function(rb_mKernel, "load_plist", rb_objc_load_plist, 1);
    
    rb_objc_define_method(rb_cObject, "to_plist", rb_objc_to_plist, 0);

    Class k = objc_getClass("NSKeyValueUnnestedProperty");
    assert(k != NULL);
    Method m = class_getInstanceMethod(k, @selector(isaForAutonotifying));
    assert(m != NULL);
    old_imp_isaForAutonotifying = method_getImplementation(m);
    method_setImplementation(m, (IMP)rb_obj_imp_isaForAutonotifying);

    reload_class_constants();
}

@interface Protocol
@end

@implementation Protocol (MRFindProtocol)
+(id)protocolWithName:(NSString *)name
{
    return (id)objc_getProtocol([name UTF8String]);
} 
@end

#if !defined(MACRUBY_STATIC)
static NSString *
get_type(NSXMLElement *elem)
{
    NSXMLNode *node = nil;
#if __LP64__
    node = [elem attributeForName:@"type64"];
#endif
    if (node == nil) {
	node = [elem attributeForName:@"type"];
	if (node == nil) {
	    return nil;
	}
    }
    return [node stringValue];
}

static void
add_stub_types(NSXMLElement *elem,
	void (*add_stub_types_cb)(SEL, const char *, bool, void *),
	void *ctx,
	bool is_objc)
{
    NSXMLNode *name = [elem attributeForName:is_objc
	? @"selector" : @"name"];
    if (name == nil) {
	return;
    }
    NSArray *ary = [elem elementsForName:@"retval"];
    if ([ary count] != 1) {
	return;
    }
    NSXMLElement *retval = [ary objectAtIndex:0];
    NSMutableString *types = [NSMutableString new];
    NSString *type = get_type(retval);
    if (type == nil) {
	return;
    }
    [types appendString:type];
    if (is_objc) {
	[types appendString:@"@:"]; // self, sel
    }
    ary = [elem elementsForName:@"arg"];
    for (NSXMLElement *a in ary) {
	type = get_type(a);
	if (type == nil) {
	    return;
	}
	[types appendString:type];
    }
    NSString *sel_str = [name stringValue];
    if (!is_objc && [ary count] > 0) {
	sel_str = [sel_str stringByAppendingString:@":"];
    }
    SEL sel = sel_registerName([sel_str UTF8String]);
    add_stub_types_cb(sel, [types UTF8String], is_objc, ctx);
}

void
rb_vm_parse_bs_full_file(const char *path,
	void (*add_stub_types_cb)(SEL, const char *, bool, void *),
	void *ctx)
{
    NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
    NSError *err = nil;
    NSXMLDocument *doc = [[NSXMLDocument alloc] initWithContentsOfURL: url
	options: 0 error: &err];
    if (doc == nil) {
	NSLog(@"can't open BridgeSupport full file at path `%s': %@",
		path, err);
	exit(1);
    }
    NSXMLElement *root = [doc rootElement];

    for (NSXMLElement *k in [root elementsForName:@"class"]) {
	for (NSXMLElement *m in [k elementsForName:@"method"]) {
	    add_stub_types(m, add_stub_types_cb, ctx, true);
	}
    }
    for (NSXMLElement *f in [root elementsForName:@"function"]) {
	add_stub_types(f, add_stub_types_cb, ctx, false);
    }
}
#endif

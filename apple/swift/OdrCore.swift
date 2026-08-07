/// The Objective-C bindings are the API; this target re-exports them so a
/// consumer writes one `import OdrCore`.
///
/// Only what an ObjC annotation cannot express belongs here — element-tree
/// sequences, real Swift optionals over the boxed style values, a thread around
/// the blocking HTTP server. Anything `NS_SWIFT_NAME`, nullability or
/// `NS_ERROR_ENUM` can say belongs in the headers, so there is one API to keep
/// correct rather than two.
@_exported import OdrCoreObjC

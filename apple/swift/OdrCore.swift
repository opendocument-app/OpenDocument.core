/// The Objective-C bindings are the API; this target re-exports them so a
/// consumer writes one `import OdrCore`.
///
/// What lives here is only what an ObjC annotation cannot express — sequences
/// over the element tree, real Swift optionals over the boxed style values, and
/// structured concurrency around the blocking HTTP server. Anything that *can*
/// be said with `NS_SWIFT_NAME`, nullability or `NS_ERROR_ENUM` belongs in the
/// headers instead, so there is one API to keep correct rather than two. This
/// is the same rule `android/` follows in refusing to restate the java API in
/// kotlin.
@_exported import OdrCoreObjC

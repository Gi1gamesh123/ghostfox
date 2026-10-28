#ifndef RUYIJS_DOCUMENT_EXCEPTION_NAMES_H
#define RUYIJS_DOCUMENT_EXCEPTION_NAMES_H
namespace ruyijs::document_model {
struct ExceptionName { const char* name; const char* constant; };
inline constexpr ExceptionName kExceptionNames[] = {
  {"IndexSizeError", "INDEX_SIZE_ERR"}, {"DOMStringSizeError", "DOMSTRING_SIZE_ERR"},
  {"HierarchyRequestError", "HIERARCHY_REQUEST_ERR"}, {"WrongDocumentError", "WRONG_DOCUMENT_ERR"},
  {"InvalidCharacterError", "INVALID_CHARACTER_ERR"}, {"NoDataAllowedError", "NO_DATA_ALLOWED_ERR"},
  {"NoModificationAllowedError", "NO_MODIFICATION_ALLOWED_ERR"}, {"NotFoundError", "NOT_FOUND_ERR"},
  {"NotSupportedError", "NOT_SUPPORTED_ERR"}, {"InUseAttributeError", "INUSE_ATTRIBUTE_ERR"},
  {"InvalidStateError", "INVALID_STATE_ERR"}, {"SyntaxError", "SYNTAX_ERR"},
  {"InvalidModificationError", "INVALID_MODIFICATION_ERR"}, {"NamespaceError", "NAMESPACE_ERR"},
  {"InvalidAccessError", "INVALID_ACCESS_ERR"}, {"ValidationError", "VALIDATION_ERR"},
  {"TypeMismatchError", "TYPE_MISMATCH_ERR"}, {"SecurityError", "SECURITY_ERR"},
  {"NetworkError", "NETWORK_ERR"}, {"AbortError", "ABORT_ERR"},
  {"URLMismatchError", "URL_MISMATCH_ERR"}, {"QuotaExceededError", "QUOTA_EXCEEDED_ERR"},
  {"TimeoutError", "TIMEOUT_ERR"}, {"InvalidNodeTypeError", "INVALID_NODE_TYPE_ERR"},
  {"DataCloneError", "DATA_CLONE_ERR"}
};
}
#endif

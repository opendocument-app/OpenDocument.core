export const error_illegal_edit_new_line = {
  code: 1,
  message: 'new line not supported by this document',
};

export let onError = (code, message) => {
  console.error(`error ${code} message ${message}`);
};

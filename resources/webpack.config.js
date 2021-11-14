const path = require('path');

module.exports = {
  entry: './src/odr.js',
  output: {
    filename: 'odr.js',
    path: path.resolve(__dirname, 'dist'),
  },
};

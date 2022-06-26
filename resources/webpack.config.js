const TerserPlugin = require('terser-webpack-plugin');
const MiniCssExtractPlugin = require('mini-css-extract-plugin');
const CssMinimizerPlugin = require('css-minimizer-webpack-plugin');

module.exports = {
  entry: {
    odr: './src/odr.js',
    odr_spreadsheet: './src/odr_spreadsheet.js',
  },
  output: {
    filename: '[name].js',
    library: '[name]',
  },
  module: {
    rules: [
      {
        test: /.s?css$/,
        use: [ MiniCssExtractPlugin.loader, 'css-loader', ],
      },
    ],
  },
  optimization: {
    minimizer: [
      new TerserPlugin({
        extractComments: false,
        terserOptions: {
          output: {
            comments: false,
          },
        },
      }),
      new CssMinimizerPlugin({
        minimizerOptions: {
          preset: [
            'default',
            {
              discardComments: { removeAll: true, },
            },
          ],
        },
      }),
    ],
  },
  plugins: [
    new MiniCssExtractPlugin(),
  ],
};

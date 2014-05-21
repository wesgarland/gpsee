/** Test should exit without segfaulting */

require.paths.push(void 0);

try
{
  require("nsm");
} catch(e){};

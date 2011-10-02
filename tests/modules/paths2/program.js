/** Test should exit without segfaulting */

require.paths.push("/nsp");
require.paths.push("/nsp2");

try
{
  require("nsm");
} catch(e){};

# **(Web)Csite**
  
This document describes the custom static site generator and its idiosyncrasies that, as of November 2025, generates the files served as my current homepage. I wrote it out of frustration with the overwhelming complexity imposed upon users of common popular alternatives. Overly simple on purpose, it does nothing more than generate an index.html file and populate it with a couple of blog posts that together form the core of the user's blog. It lacks many features that you might expect, most prominently tags. In addition, posts are written using HTML tags directly instead of relying on some intermediate format like Markdown. I intend to elaborate on my rationale for doing so some other time.
  
For our site to successfully build, the following two prerequisites exist:  
1. A `content/` directory that is tracked by Git
2. *At least* one file in said directory called index.htm that serves as our blog's entry point

Content that is to be served as an HTML page has to reside in our `content/` directory and have a `.htm` extension[^1]. Everything else will simply be copied as is. The default output directory containing all of our site's content will end up in `docs/` by default. Both directories are customizable through passing appropriate values to the underlying Makefile used to build our site. `_SITE_EXT_SOURCE_DIR` can be set to decide upon an alternative content path while `_SITE_EXT_TARGET_DIR` will set a different output path.
  
`.htm` files inside the `content/` directory use HTML tags and are expected to have two header fields: title and subtitle. Both field names are followed by a colon, some white space and finally the field value. These headers are separated by a single newline.
  
A certain degree of templating can be achieved through placing .htm files inside the `content/blocks/` directory. In order to use these (building) blocks during the build process, the source of the static site generator itself has to be modified. `menu.htm` is included as a reference for how to do so.
  
I am sure inventive minds can come up with a more convoluted approach, but the currently intended way to use Csite is either to place content directly alongside its source or keep it as a plain dependency of the repository that contains the source of the website to be built. For the latter case, using git-subtree might be a viable approach.
  
To build pages just run `make`.

For everything else, the actual source code might tell a better story than this README ever could. Csite is intended to be used as [malleable](http://forum.malleable.systems/t/ink-switch-malleable-software-essay/340/5) software. Its purpose is to serve as a starting point to being customize able as much as needed with as little friction as possible.

[^1]: [Historically](https://en.wikipedia.org/wiki/HTML#Naming_conventions), the `.htm` extension was used to denote proper HTML files on MS-DOS and thus still has great editor support today. We went with the three-letter abbreviation for posts as it conveys the intention that these are not yet complete HTML files.

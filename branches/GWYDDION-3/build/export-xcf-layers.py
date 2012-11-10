# This is not a script.  It must be executed from within GIMP python-fu-eval
# using execfile().  This is usually done using export-xcf-layers.sh.  The
# caller may/must set:
# cfgfile (mandatory) -- the name of configuration file to use
# srcdir (optional, defaults to '.') -- directory with XCF images
# destdir (optional, defaults to '.') -- directory where to write PNG images
from ConfigParser import RawConfigParser
import sys, os, gimpfu

if 'srcdir' not in globals():
    srcdir = '.'
if 'destdir' not in globals():
    destdir = '.'

config = RawConfigParser()
config.read(cfgfile)

for xcf in config.sections():
    xcffile = os.path.join(srcdir, xcf + '.xcf')
    for ext in ('.bz2', '.gz', ''):
        if os.access(xcffile + ext, os.R_OK):
            xcffile += ext
            break
    sys.stdout.write('%s:' % xcf)
    sys.stdout.flush()
    for png in config.options(xcf):
        layerlist = [x.strip() for x in config.get(xcf, png).split(';')]
        png = png.strip()
        pngfile = os.path.join(destdir, png + '.png')
        sys.stdout.write(' %s' % png)
        sys.stdout.flush()

        if ext == '.bz2':
            image = pdb.file_bz2_load(xcffile, xcffile)
        elif ext =='.gz':
            image = pdb.file_gz_load(xcffile, xcffile)
        else:
            image = pdb.gimp_xcf_load(0, xcffile, xcffile)

        for layer in image.layers:
            if layer.name in layerlist:
                pdb.gimp_item_set_visible(layer, True)
            else:
                pdb.gimp_item_set_visible(layer, False)

        newlayer = pdb.gimp_image_merge_visible_layers(image,
                                                       gimpfu.CLIP_TO_IMAGE)
        pdb.file_png_save2(image, newlayer, pngfile, pngfile,
                           False, 9,
                           False, False, False, False, False, False, False)
    sys.stdout.write('\n')
    sys.stdout.flush()


#include <stdio.h>
#include <string.h>
#include <libgwyddion/gwymath.h>
#include <libprocess/gwyprocess.h>

int
main(void)
{
    GwyInventory *gvalues;
    gint n, i;

    printf("<informaltable frame='none' id='table-grain-quantities'>\n"
           "  <tgroup cols='3'>\n"
           "    <?dblatex lll?>\n"
           "    <thead>\n"
           "      <row>\n"
           "        <entry>Symbol</entry>\n"
           "        <entry>Group</entry>\n"
           "        <entry>Name</entry>\n"
           "      </row>\n"
           "    </thead>\n"
           "    <tbody>\n");

    gwy_process_type_init();
    gvalues = gwy_grain_values();
    n = gwy_inventory_get_n_items(gvalues);
    for (i = 0; i < n; i++) {
        GwyGrainValue *gval = gwy_inventory_get_nth_item(gvalues, i);
        GwyGrainValueGroup group = gwy_grain_value_get_group(gval);

        if (group == GWY_GRAIN_VALUE_GROUP_ID
            || group == GWY_GRAIN_VALUE_GROUP_USER)
            continue;

        printf("      <row>\n"
               "        <entry><varname>%s</varname></entry>\n"
               "        <entry>%s</entry>\n"
               "        <entry>%s</entry>\n"
               "      </row>\n",
               gwy_grain_value_get_symbol(gval),
               gwy_grain_value_group_name(group),
               gwy_resource_get_name(GWY_RESOURCE(gval)));
    }

    printf("    </tbody>\n"
           "  </tgroup>\n"
           "</informaltable>\n");

    return 0;
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

/*
 *  $Id$
 *  Copyright (C) 2011 David Nečas (Yeti).
 *  E-mail: yeti@gwyddion.net.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include "testlibgwy.h"

/***************************************************************************
 *
 * Random number generation
 *
 ***************************************************************************/

// NB: For any meaningful testing niter should be at least twice the state
// size.

#define P_500_25 .893250457965680
enum { bit_samples_taken = 1000 };

void
test_rand_reproducibility_seed(void)
{
    enum { niter = 1000, nseed = 1000 };

    GwyRand *rng0 = gwy_rand_new_with_seed(0);

    for (guint seed = 0; seed < nseed; seed++) {
        GwyRand *rng1 = gwy_rand_new_with_seed(seed);
        gwy_rand_set_seed(rng0, seed);

        for (guint i = 0; i < niter; i++) {
            guint64 x = gwy_rand_int(rng0);
            guint64 y = gwy_rand_int(rng1);
            g_assert_cmpuint(x, ==, y);

            x = gwy_rand_byte(rng0);
            y = gwy_rand_byte(rng1);
            g_assert_cmpuint(x, ==, y);

            x = gwy_rand_int64(rng0);
            y = gwy_rand_int64(rng1);
            g_assert_cmpuint(x, ==, y);

            x = gwy_rand_boolean(rng0);
            y = gwy_rand_boolean(rng1);
            g_assert_cmpuint(x, ==, y);

            gdouble xd = gwy_rand_double(rng0);
            gdouble yd = gwy_rand_double(rng1);
            g_assert_cmpfloat(xd, ==, yd);
        }

        gwy_rand_free(rng1);
    }

    gwy_rand_free(rng0);
}

void
test_rand_reproducibility_static(void)
{
    static const guint64 seed1 = G_GUINT64_CONSTANT(0x42);
    static const guint64 seed2 = G_GUINT64_CONSTANT(0x4200000000);
    static const guint64 seed3 = G_GUINT64_CONSTANT(0x4200000042);
    static const guint32 sequence1[] = {
        2928813469u, 4259796625u, 3849829594u, 895311364u, 1062222073u,
        3103062284u, 1572883362u, 1046095897u, 3717188631u, 1790260520u,
        70964011u, 581083473u, 3991323890u, 2596036662u, 92359043u,
        2699133385u, 3957708626u, 120872815u, 219273978u, 1774978889u,
        3539674155u, 2462645739u, 72820333u, 1849205572u, 1423824306u,
        2916447579u, 987813887u, 2675687012u, 2136169428u, 2771461677u,
        3315654927u, 3210510479u, 1316778597u, 2370609180u, 3877402988u,
        1652964189u, 3385324625u, 688818787u, 2787296680u, 4263761192u,
        1310388148u, 205493076u, 1829592673u, 1665741611u, 1844409009u,
        1572444904u, 2060320348u, 780644664u, 3852180654u, 3954811852u,
        2282126218u, 826326029u, 4290076931u, 2165883725u, 8774695u,
        1525472021u, 429878885u, 428153468u, 3478129474u, 2024416565u,
        2360093210u, 3732809025u, 3758520396u, 617266043u, 1256555256u,
        2685481397u, 3476236505u, 2229500028u, 2014256645u, 203651760u,
        1546894865u, 3265912299u, 1916242034u, 3981890105u, 3289402184u,
        3914132476u, 1771893256u, 370604797u, 2235578681u, 1842144917u,
        2203341720u, 3276621791u, 3408400794u, 326104125u, 1840244053u,
        2145357643u, 2725443992u, 3488110195u, 1714186437u, 1089500347u,
        2661789959u, 1426186024u, 2354562413u, 3843036962u, 977340763u,
        3951442645u, 860027933u, 1537770036u, 2250818984u, 2476244897u,
        3502749487u, 2697669927u, 2709435059u, 3808205428u, 4146940371u,
        3074071705u, 3638494217u, 1593832312u, 3270749017u, 4096044194u,
        931648729u, 1617838102u, 2762926318u, 2266938796u, 1819078274u,
        4224421648u, 2047224778u, 3847531566u, 1677332157u, 2963772029u,
        355967202u, 2112937020u, 557818081u, 1009859467u, 1086619987u,
        1652661585u, 3365920730u, 953445648u, 1030070720u, 3566609185u,
        2765760054u, 1645358556u, 2967366416u, 3614565107u, 3428519171u,
        1442804162u, 2190984399u, 1512487064u, 2431171480u, 3193215193u,
        2220202622u, 2000870983u, 2355288123u, 1406810454u, 779499230u,
        1418932592u, 769395545u, 2177473031u, 375868778u, 3673634662u,
        3866779344u, 2211924943u, 879375503u, 1525423807u, 2657789133u,
        401443037u, 1469215370u, 1988996493u, 1575665495u, 1506732636u,
        3795045643u, 1621018263u, 1772231692u, 3051173672u, 2791232091u,
        3911249322u, 82542915u, 185205464u, 2397653901u, 136793972u,
        1255692199u, 4258320929u, 1787660490u, 3252377333u, 435214498u,
        1600511693u, 3619332696u, 1093744833u, 583053390u, 1142839957u,
        2283306262u, 1917369885u, 104029241u, 174251828u, 3469001141u,
        3441836399u, 2285358108u, 2988297402u, 4085929448u, 1026021484u,
        1348314349u, 185223057u, 4103016901u, 224253814u, 3468280712u,
        2892109049u, 1719838256u, 1742922493u, 372302927u, 1859975517u,
        3304460790u, 599508311u, 1946381102u, 3498406316u, 175667182u,
        35708075u, 3756559625u, 46644599u, 2999352728u, 104825416u,
        208374413u, 2660446144u, 439823838u, 612982038u, 2658370726u,
        1571141750u, 290183077u, 703198160u, 273930045u, 2376994934u,
        351996624u, 2866053875u, 337098591u, 2011343034u, 1115107907u,
        1879150942u, 3738622427u, 3304326973u, 2769764458u, 2393355443u,
        3119092566u, 2700207269u, 4056302317u, 865761282u, 3565895646u,
        751781608u, 2042133431u, 1088598248u, 3008810u, 1129269979u,
        844132786u, 1594114734u, 1871245002u, 1047192066u, 3712279781u,
        3436348003u, 778125957u, 1417914020u, 1072087468u, 2909509808u,
        2677666327u, 743626078u, 2411783502u, 211366738u, 2343940314u,
        94967726u, 1678171682u, 53635071u, 2235974812u, 3299057113u,
        4160642432u, 3080786695u, 3963468125u, 2482733878u, 891756173u,
        2411819354u, 3484557375u, 2335823759u, 857578309u, 1531438791u,
        2200035596u, 3298806490u, 2736572787u, 1659345655u, 1389463695u,
        1276436918u, 4139165086u, 324578267u, 2943409981u, 3856225491u,
        1447308044u, 2215239060u, 2615943335u, 841306423u, 3649457868u,
        1776510535u, 3409660766u, 3886403026u, 439401629u, 3909593727u,
        3428804118u, 933154374u, 1562272482u, 384235353u, 2463160459u,
        2011941076u, 213082009u, 4216542254u, 2104054669u, 3093247857u,
        2448921332u, 1847939838u, 2092091854u, 2141816480u, 1224889357u,
        3347042958u, 1406718245u, 4044666374u, 2147746166u, 1277481846u,
        4132682713u, 998019778u, 841058578u, 444033929u, 763061029u,
        433919892u, 4248946085u, 3382440774u, 4100009029u, 2313042045u,
        4197866934u, 873280717u, 3155982197u, 1489035354u, 852672191u,
        3716562535u, 754361980u, 1929841835u, 965296727u, 1232179333u,
        544365434u, 1093834331u, 2721758888u, 2140592869u, 4203914684u,
        573455776u, 3966217897u, 3463799221u, 3766015471u, 1541184070u,
        4025958384u, 1571383747u, 2713156080u, 2327837301u, 3577062292u,
        3361637965u, 917421447u, 3296451711u, 593755293u, 1011559142u,
        2938244662u, 2795148171u, 3263319807u, 4030613787u, 2041236372u,
        270680748u, 3037970488u, 523807486u, 1748860147u, 1292806762u,
        639334072u, 2665560522u, 943728340u, 4285552434u, 3483230732u,
        1890077737u, 121001361u, 2843335310u, 1335393830u, 2946976081u,
        403535057u, 2197091087u, 3222654181u, 1251722672u, 4244203823u,
        55333356u, 49409532u, 3694186513u, 3335725957u, 3641840220u,
        1577495249u, 1232799937u, 2657712928u, 1850758327u, 2687393176u,
        498037139u, 4028903201u, 1005728536u, 3503573359u, 1193839261u,
        1188887251u, 4086998526u, 784715092u, 3856944255u, 2684011202u,
        1195370306u, 1164939724u, 3242688440u, 2324844001u, 1155100637u,
        1404721012u, 2252880635u, 1035689637u, 1710929362u, 1649786735u,
        2372434706u, 2177529624u, 3215653882u, 1594382413u, 802508800u,
        507868555u, 2690573772u, 866842190u, 4218789490u, 3305736152u,
        2794134401u, 2756992254u, 3038150684u, 2865797894u, 1906805298u,
        4049250450u, 3593405258u, 3804080743u, 1280777554u, 3489614126u,
        775768499u, 1946862244u, 4271836164u, 3272838844u, 3119264541u,
        3312529799u, 1466963929u, 2576638106u, 2247864656u, 1423963160u,
        1666382148u, 2097650855u, 1806221192u, 1584324103u, 4164309153u,
        925125638u, 152417347u, 3449729606u, 1808975163u, 2117310454u,
        1482986902u, 2591822700u, 3562387276u, 414132040u, 3488181477u,
        3452857783u, 1474238187u, 145384511u, 3274115814u, 2086316910u,
        2788870095u, 3686290118u, 2914811459u, 413962646u, 1765213630u,
        2213065323u, 1144472170u, 4288587909u, 4177713684u, 3979305217u,
        3733513578u, 2670777477u, 447987782u, 3812986044u, 3750214299u,
        1394371384u, 3555125373u, 3720392084u, 2395913067u, 2720874032u,
        786798317u, 3235938735u, 101949218u, 2654897008u, 3380876935u,
        3599655450u, 2738199494u, 4079328770u, 3692600882u, 1012748175u,
        1379776519u, 2358539336u, 4150457016u, 3161621103u, 1483548926u,
        2860765389u, 821067416u, 3480649778u, 2522144705u, 3526393201u,
        633091561u, 2734993311u, 2302172180u, 3734145633u, 2915461730u,
        4098354320u, 2752637037u, 3685015891u, 1168960097u, 2178362187u,
        3286927913u, 2100174793u, 717209335u, 742536749u, 2079403523u,
        3110536303u, 2511999848u, 3494088381u, 3536973084u, 2114976703u,
        2064279393u, 2511453145u, 3374577844u, 2111626553u, 628606588u,
    };
    static const guint32 sequence2[] = {
        9910173u, 121945262u, 3889925889u, 528971828u, 3860988962u,
        155597482u, 4183532641u, 2012769111u, 4202864221u, 187410096u,
        2188235899u, 3097790475u, 14012866u, 1833544229u, 605208047u,
        646242928u, 323469090u, 584459218u, 429721532u, 1139879823u,
        2636753812u, 4159071550u, 3485627678u, 2346945779u, 3701555587u,
        1409712992u, 3913497184u, 1523731845u, 1787984859u, 2191844399u,
        3433157201u, 3647858108u, 1348217931u, 2812076131u, 205142758u,
        809854052u, 1536930523u, 2861779004u, 3081895125u, 4069624181u,
        1699875924u, 919326464u, 229434543u, 2110516946u, 146507734u,
        3044109820u, 157052044u, 245404747u, 2037151633u, 679393722u,
        1544701482u, 2488955064u, 2407694539u, 1079431116u, 1541566868u,
        3654248911u, 2786544972u, 2984151608u, 522627525u, 3282463702u,
        1886567135u, 3175748632u, 3652005262u, 3410216355u, 2084693356u,
        2531316386u, 2919457182u, 2931834760u, 4241418815u, 2157260741u,
        4040261627u, 3292257812u, 483576691u, 2269191732u, 2732807326u,
        1919979786u, 3096618898u, 3532488837u, 2353858220u, 2450621270u,
        2678320963u, 671148324u, 1253392576u, 1085480426u, 672896384u,
        2430116810u, 1090598528u, 2997971800u, 1615019533u, 1945754511u,
        536774280u, 137001720u, 3278144617u, 1148310931u, 2064766412u,
        1440494799u, 642934080u, 2344850002u, 1587016982u, 1446560022u,
        4230733852u, 4145646699u, 3288569551u, 1602273903u, 4256359163u,
        4093242154u, 36292317u, 1326354368u, 3221901978u, 880141168u,
        1549406579u, 2747505692u, 2043270100u, 154282564u, 2028986672u,
        2328934999u, 3942842100u, 3458152249u, 294310801u, 3883346801u,
        1597666159u, 449523906u, 301019825u, 3164908416u, 750720848u,
        2498379182u, 1335202480u, 4213154655u, 3363480128u, 1907486020u,
        3317106982u, 3350931989u, 380943462u, 623102607u, 994797281u,
        1063279967u, 4060538877u, 4109131237u, 589528544u, 3738571621u,
        136090820u, 1134709857u, 3743728068u, 181164898u, 3177942843u,
        1530771476u, 1132305893u, 3681787441u, 439106807u, 3752709752u,
        1597562857u, 1245790040u, 637054706u, 77076877u, 3645956109u,
        3102758846u, 1390100627u, 2124740296u, 1250865393u, 1948760869u,
        138635185u, 3070958199u, 281862978u, 2623350043u, 3703816487u,
        3136476963u, 357499482u, 1804531734u, 2644062981u, 3670384869u,
        3789476058u, 3542729729u, 778459782u, 3903400247u, 1124750797u,
        701197987u, 867475459u, 1519917697u, 1200993732u, 3824275722u,
        2120944096u, 1062077524u, 2424748566u, 2822000507u, 3242519391u,
        1791243043u, 3644191733u, 16534505u, 2031243872u, 3903678190u,
        159242785u, 3848121991u, 4161948180u, 3705419583u, 2626065795u,
        1681432496u, 1031313844u, 1116819488u, 300056746u, 86145480u,
        1309947831u, 4042549066u, 1468306280u, 2405612507u, 2789702688u,
        3782446435u, 2161232348u, 3002397710u, 3578178622u, 2344430761u,
        2730907408u, 4260403394u, 3453786476u, 3105452105u, 4180296658u,
        2621053455u, 2674100321u, 408206807u, 688498182u, 730621650u,
        1487656719u, 3474750051u, 701328160u, 2451605069u, 3171281518u,
        3432905861u, 1227624318u, 2550117398u, 289899669u, 204224127u,
        3591735284u, 2052089850u, 2141286294u, 2702965364u, 3434722245u,
        3789569709u, 2818536137u, 3589539437u, 13856845u, 4193465769u,
        1759628909u, 1533729247u, 2556292984u, 2171664560u, 3407386554u,
        3960355022u, 770978055u, 1497925720u, 3714556787u, 2326591340u,
        3817838568u, 1961859154u, 2370356482u, 2017089530u, 1791190130u,
        2264734552u, 2234638368u, 2778983029u, 3112235885u, 2566219290u,
        3295675285u, 1455851170u, 1538419775u, 4249108453u, 3146751962u,
        3541872010u, 139344787u, 2501783554u, 2496448116u, 3312628541u,
        1764797540u, 2203132649u, 1481467907u, 833827079u, 781803454u,
        1939450937u, 2528261444u, 3593535125u, 3540915057u, 435161112u,
        610259863u, 2123988820u, 2561575996u, 2134888450u, 3745829448u,
        1525875106u, 2133944871u, 4287580001u, 739914355u, 2884777107u,
        2611671364u, 3289994183u, 425896473u, 2086167001u, 4075634988u,
        687798708u, 2594343810u, 1501101674u, 2175526248u, 496291151u,
        409914668u, 3498120788u, 3457051154u, 973657989u, 4185780936u,
        987777123u, 1256224864u, 3582226951u, 1607851021u, 367578585u,
        705946826u, 4024199538u, 3145887102u, 267831345u, 3637670839u,
        553701800u, 1397216078u, 3694193875u, 506666798u, 2136888255u,
        3963832836u, 1007262365u, 3153179151u, 1275987095u, 3984603620u,
        177797449u, 1377365991u, 3365745405u, 3653985487u, 315179633u,
        780063686u, 1573769390u, 3039571501u, 3855718326u, 2832136358u,
        3818331328u, 1474955352u, 3125678330u, 1865298003u, 2235831586u,
        500388107u, 1625392594u, 2093174552u, 178747007u, 3232785535u,
        615092257u, 256802738u, 1102302043u, 3747131527u, 3988263933u,
        2150845715u, 2969327154u, 1747717360u, 2778242783u, 2225635703u,
        452633312u, 1733003270u, 297030655u, 2999904000u, 3241108018u,
        2304722124u, 1206112135u, 3840869891u, 2953462685u, 301400244u,
        4214038880u, 296666999u, 761422566u, 2913100474u, 2410870468u,
        3228628243u, 3710190218u, 3742636840u, 3546933206u, 3999117411u,
        2060342540u, 69470725u, 2668425740u, 1035054233u, 2237716826u,
        3899905593u, 3820231865u, 1775526334u, 282740214u, 369422589u,
        298398115u, 702825527u, 3111332086u, 1009175843u, 472944304u,
        169690392u, 1654897962u, 2826327109u, 1730785355u, 3044983018u,
        635087119u, 4185631254u, 1850501543u, 3581012551u, 1750872152u,
        2527551889u, 2088011408u, 1660056704u, 4152910629u, 822149100u,
        2517803348u, 2265507712u, 1318234646u, 2019047058u, 2181318517u,
        2719541699u, 584764976u, 2299386843u, 4233112482u, 124982845u,
        2042327864u, 1628450601u, 618338628u, 912579976u, 1337884181u,
        3838651437u, 1426131343u, 3342425578u, 2795603062u, 3617081399u,
        2669670546u, 1612084866u, 4135431265u, 1874257614u, 2192149115u,
        3935848705u, 573398930u, 147689719u, 4083896435u, 1201991632u,
        2183308947u, 2736498225u, 297467470u, 1869354403u, 3320663028u,
        891219728u, 1816993754u, 3448969292u, 3838069412u, 2266568211u,
        303098805u, 1253690328u, 3696316820u, 2552761846u, 3047697091u,
        594071158u, 2733037019u, 2725060132u, 3070243262u, 2792934652u,
        1237477188u, 2641750794u, 3145748216u, 350625183u, 2056641610u,
        3964002942u, 1922491452u, 1053248656u, 1272024572u, 8287300u,
        1501414602u, 1794240631u, 927922610u, 3364684736u, 4016496041u,
        868666274u, 101936533u, 63347320u, 3848660363u, 1700727846u,
        2347314144u, 2057495742u, 2515840909u, 2695003685u, 1149059158u,
        2745945440u, 3562500239u, 311878827u, 1436935163u, 2424766286u,
        1109887185u, 1748300204u, 3124815288u, 1701297414u, 47849678u,
        2295367500u, 2873419393u, 1858143554u, 135050513u, 1657341471u,
        479163897u, 2012740146u, 3617268328u, 2566293178u, 3277560074u,
        555295413u, 3891030868u, 1406325849u, 1275452696u, 3689896460u,
        817298528u, 2234473211u, 1442774029u, 823451552u, 2054061260u,
        4240570580u, 2457153769u, 528941732u, 1663906587u, 1666128352u,
        3874380162u, 1357769972u, 4193147189u, 2819618602u, 2764900374u,
    };
    static const guint32 sequence3[] = {
        2899446341u, 975130052u, 734027272u, 2243826908u, 335705156u,
        899202757u, 980052852u, 1620737839u, 1680333648u, 2569390473u,
        1338368482u, 536665439u, 4219990268u, 2460196777u, 478621259u,
        1697504893u, 3247953340u, 1174539479u, 1091701018u, 3006761459u,
        2069666848u, 1768842050u, 3702514717u, 2072283772u, 2627755618u,
        1583631113u, 20458794u, 3274384202u, 2857712735u, 2570810314u,
        2997390724u, 3109144441u, 788749131u, 4059127676u, 1649543410u,
        1964905008u, 800549772u, 3263011499u, 713261169u, 889456704u,
        3550939465u, 2342216514u, 1162739946u, 3398547847u, 626515276u,
        2228626049u, 3864626459u, 1529416018u, 2536359811u, 2483222617u,
        1760202052u, 313041998u, 1622601691u, 1261938551u, 1957069835u,
        1737635942u, 2276717799u, 2496056780u, 3598919153u, 2807051088u,
        3930407315u, 1312297201u, 863357633u, 533626124u, 2449315149u,
        1940393563u, 2078491137u, 1856467317u, 3049398834u, 300669615u,
        3921950376u, 1333079168u, 3489428891u, 2062169981u, 1896458316u,
        3550258038u, 3154126416u, 2523537525u, 114122869u, 703776947u,
        3480197029u, 4045412005u, 1109646952u, 3933512106u, 1043844719u,
        1166046171u, 3655269840u, 3946325277u, 926359707u, 2002862835u,
        1560549791u, 566028591u, 2710317298u, 1696512648u, 3908839022u,
        1010564230u, 2029780067u, 741219476u, 3017199808u, 4253247199u,
        710557534u, 320462056u, 570108570u, 1803760784u, 3820043577u,
        1484484362u, 4238273656u, 2426734870u, 156649603u, 4280983346u,
        1369812821u, 3479297697u, 1731570556u, 2088930697u, 4015603443u,
        489237861u, 3046923997u, 977752856u, 157978049u, 3629301913u,
        3659467792u, 2355214528u, 285258217u, 1630412840u, 524560335u,
        1944633746u, 2546919722u, 3680302318u, 2853885892u, 3357094768u,
        4110477669u, 2590535236u, 3150031685u, 1882443362u, 1194301269u,
        1921375077u, 2736563545u, 2613046105u, 694791744u, 1802146432u,
        3336042509u, 752964506u, 1508120556u, 1776752787u, 3285640376u,
        3163475567u, 3240821523u, 1280855713u, 383071729u, 1501030807u,
        2173668186u, 1317111525u, 4103977791u, 3663330442u, 1592614829u,
        3508713826u, 940440761u, 2241089748u, 1878689035u, 4220644761u,
        3790021570u, 732575578u, 141412080u, 2881476322u, 1441940085u,
        496360233u, 1741244156u, 2582155731u, 3817800182u, 982471475u,
        2227588838u, 3604934199u, 2217683521u, 3312492016u, 277863923u,
        853823622u, 3178854837u, 2543296371u, 496593173u, 2750345415u,
        1484121773u, 2920087707u, 2295514781u, 3775808531u, 1929146975u,
        4138074283u, 4166749091u, 2138255130u, 3117140863u, 2304556473u,
        4139220562u, 1828824231u, 3258431781u, 3765288443u, 3332633022u,
        280107856u, 1361791617u, 3979804487u, 3967361787u, 3102331460u,
        2875125613u, 4093314038u, 3653158079u, 3237201535u, 801051184u,
        1881378018u, 2516494801u, 4207289405u, 3375475999u, 4042103741u,
        3754218533u, 439051930u, 210206311u, 33595150u, 2884567160u,
        868010184u, 3622025144u, 1042858607u, 2647274237u, 857090685u,
        4040402922u, 415047760u, 1786354472u, 1285061041u, 1666577194u,
        3576819534u, 201831489u, 1351145628u, 1272340340u, 2570993775u,
        2432462847u, 1088204687u, 138237003u, 2771487397u, 1753561572u,
        2492514121u, 2926623503u, 3952697011u, 1408660028u, 1037188928u,
        807150697u, 1768885015u, 362262562u, 1900911133u, 1219692824u,
        1664228653u, 887409052u, 3001599745u, 177091033u, 3530848226u,
        3564997393u, 1262086687u, 2209883293u, 1034727292u, 1487629350u,
        3410689913u, 1710288452u, 1265226635u, 298923925u, 587960304u,
        750041390u, 1509339312u, 1321031912u, 1069302779u, 3004240551u,
        3367964768u, 1481044519u, 277549595u, 2226127207u, 928607287u,
        2998642439u, 4273213389u, 225558035u, 2471672282u, 1614087417u,
        1731616325u, 1605078214u, 672248287u, 2145826702u, 76370400u,
        3563461635u, 493506606u, 774912851u, 4222282990u, 2137198169u,
        1416366515u, 1270197553u, 1801259190u, 2892319400u, 3326805726u,
        4024753520u, 325115646u, 2661901840u, 2480386250u, 636839185u,
        3585526406u, 3076148756u, 3913766816u, 3534820362u, 2000040797u,
        627241557u, 2740750620u, 1595877262u, 4067107357u, 1892609375u,
        3550716543u, 695299441u, 73995432u, 2214277401u, 3514063197u,
        2565345211u, 2532548045u, 416431932u, 2827935514u, 498050743u,
        4268569477u, 337095906u, 3873516018u, 3971984079u, 874111379u,
        13987600u, 3528509460u, 418368198u, 1997032548u, 1984805687u,
        2969381119u, 3384836994u, 1559432094u, 4149050141u, 2451278898u,
        1811188526u, 3342415477u, 3214093491u, 875337302u, 3165865409u,
        2157397437u, 365062623u, 2274305735u, 3580575034u, 2553404296u,
        2894538640u, 845003902u, 1955023876u, 1040660431u, 2756245101u,
        4114911596u, 2795030442u, 1321922470u, 26260219u, 2803696297u,
        1234800781u, 872359391u, 1006303431u, 3775112533u, 3965180213u,
        2252537933u, 1042582143u, 2370254341u, 825047950u, 3303256899u,
        3708863037u, 3008803410u, 3707716383u, 1362426951u, 1034061788u,
        1172591890u, 2052957895u, 1093153002u, 3460449506u, 4009394729u,
        2519338056u, 3163417427u, 3479017709u, 2805888267u, 1413642735u,
        4041987911u, 2693946062u, 448273560u, 4158657602u, 715885771u,
        2798919020u, 196745198u, 584058177u, 1615672679u, 1705900517u,
        3924786968u, 3959134279u, 4154099999u, 559969361u, 1566911525u,
        281737882u, 1081670021u, 898950760u, 645962402u, 3744670870u,
        1383783518u, 3468149400u, 452889637u, 489353394u, 1197970717u,
        2094180480u, 981143624u, 893390637u, 1357004968u, 1101446819u,
        333373785u, 2266994630u, 3829786170u, 3394448656u, 1559832731u,
        3236722114u, 3365624777u, 1615126452u, 16320101u, 3287026474u,
        1024942719u, 3764639773u, 1413686272u, 3003345951u, 877331701u,
        2648006826u, 869581575u, 377539464u, 234980578u, 282613099u,
        3904982944u, 3694666385u, 1860651709u, 1068259647u, 1528494596u,
        1028408117u, 2107246728u, 2312063419u, 4274626959u, 865248121u,
        2954533408u, 1417313749u, 2344293078u, 332506128u, 3456048943u,
        1837180938u, 2963226180u, 1779557333u, 4151494082u, 4131799112u,
        3165330860u, 3792368330u, 1587749446u, 3934239969u, 238108667u,
        1532418243u, 207298339u, 3836944276u, 1603553245u, 777837041u,
        2392561860u, 1612201394u, 516727667u, 416807413u, 373541293u,
        2053246538u, 4065622162u, 3418818891u, 2458924743u, 4270813819u,
        3378050622u, 553866858u, 2187874754u, 953157986u, 937536362u,
        3653342085u, 432613055u, 2158969142u, 2636832050u, 1157163656u,
        1700097424u, 957977595u, 2697603316u, 2776673841u, 1871940853u,
        357439492u, 1963016790u, 1057954236u, 3601059895u, 3145574393u,
        1356379353u, 772469026u, 3289218702u, 908291955u, 1204702225u,
        3205682875u, 699365571u, 4129127820u, 2193194253u, 715357054u,
        4285851268u, 1135739683u, 3032851409u, 108049315u, 754392768u,
        2731255069u, 1797442324u, 3852753639u, 3552676976u, 164541354u,
        3693200621u, 4154213989u, 1708388732u, 118706546u, 3596818600u,
        366716608u, 3061989396u, 3619495010u, 1509154498u, 4209870931u,
        883540235u, 3515801781u, 3863002449u, 3909223952u, 3839566873u,
    };

    GwyRand *rng = gwy_rand_new_with_seed(0);

    gwy_rand_set_seed(rng, seed1);
    for (guint i = 0; i < G_N_ELEMENTS(sequence1); i++)
        g_assert_cmpuint(gwy_rand_int(rng), ==, sequence1[i]);

    gwy_rand_set_seed(rng, seed2);
    for (guint i = 0; i < G_N_ELEMENTS(sequence2); i++)
        g_assert_cmpuint(gwy_rand_int(rng), ==, sequence2[i]);

    gwy_rand_set_seed(rng, seed3);
    for (guint i = 0; i < G_N_ELEMENTS(sequence3); i++)
        g_assert_cmpuint(gwy_rand_int(rng), ==, sequence3[i]);

    gwy_rand_free(rng);
}

static int
compare_uints(const void *va, const void *vb)
{
    const guint32 *pa = (const guint32*)va, *pb = (const guint32*)vb;
    guint32 a = *pa, b = *pb;

    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

void
test_rand_difference_seed(void)
{
    enum { niter = 16, nseed = 100000 };

    GwyRand *rng = gwy_rand_new_with_seed(0);

    guint32 *values = g_new(guint32, niter*nseed);
    for (guint seed = 0; seed < nseed; seed++) {
        gwy_rand_set_seed(rng, seed);
        for (guint i = 0; i < niter; i++)
            values[seed*niter + i] = gwy_rand_int(rng);
    }

    qsort(values, nseed, niter*sizeof(guint32), compare_uints);

    for (guint seed = 0; seed+1 < nseed; seed++) {
        guint32 *thisval = values + seed*niter,
                *nextval = thisval + niter;
        gboolean equal = TRUE;
        for (guint i = niter; i; i--) {
            if (*(thisval++) != *(nextval++)) {
                equal = FALSE;
                break;
            }
        }
        g_assert(!equal);
    }

    g_free(values);
    gwy_rand_free(rng);
}

void
test_rand_difference_auto(void)
{
    enum { niter = 16, nseed = 100000 };

    guint32 *values = g_new(guint32, niter*nseed);
    for (guint seed = 0; seed < nseed; seed++) {
        GwyRand *rng = gwy_rand_new();
        for (guint i = 0; i < niter; i++)
            values[seed*niter + i] = gwy_rand_int(rng);
        gwy_rand_free(rng);
    }

    qsort(values, nseed, niter*sizeof(guint32), compare_uints);

    for (guint seed = 0; seed+1 < nseed; seed++) {
        guint32 *thisval = values + seed*niter,
                *nextval = thisval + niter;
        gboolean equal = TRUE;
        for (guint i = niter; i; i--) {
            if (*(thisval++) != *(nextval++)) {
                equal = FALSE;
                break;
            }
        }
        g_assert(!equal);
    }

    g_free(values);
}

void
test_rand_copy(void)
{
    enum { niter = 10000 };

    GwyRand *rng = gwy_rand_new_with_seed(g_test_rand_int());
    GwyRand *rng0 = gwy_rand_copy(rng);

    for (guint i = 0; i < niter; i++) {
        guint x = gwy_rand_int(rng);
        guint y = gwy_rand_int(rng0);
        g_assert_cmpuint(x, ==, y);
    }

    GwyRand *rng1 = gwy_rand_copy(rng);

    for (guint i = 0; i < niter; i++) {
        guint x = gwy_rand_int(rng);
        guint y = gwy_rand_int(rng1);
        g_assert_cmpuint(x, ==, y);
    }

    gwy_rand_assign(rng0, rng);
    for (guint i = 0; i < niter; i++) {
        guint x = gwy_rand_int(rng);
        guint y = gwy_rand_int(rng0);
        g_assert_cmpuint(x, ==, y);
    }

    gwy_rand_free(rng1);
    gwy_rand_free(rng0);
    gwy_rand_free(rng);
}

void
test_rand_range_boolean(void)
{
    enum { niter = 10000 };
    GwyRand *rng = gwy_rand_new_with_seed(g_test_rand_int());

    for (guint i = 0; i < niter; i++) {
        guint x = gwy_rand_boolean(rng);
        g_assert_cmpuint(x, <=, 1);
    }

    gwy_rand_free(rng);
}

void
test_rand_range_byte(void)
{
    enum { niter = 100000 };
    GwyRand *rng = gwy_rand_new_with_seed(g_test_rand_int());

    for (guint i = 0; i < niter; i++) {
        guint x = gwy_rand_byte(rng);
        g_assert_cmpuint(x, <, 0x100);
    }

    gwy_rand_free(rng);
}

void
test_rand_range_int(void)
{
    enum { niter = 10000, nrange = 100 };
    GwyRand *rng = gwy_rand_new_with_seed(0);

    for (guint r = 0; r < nrange; r++) {
        gwy_rand_set_seed(rng, g_test_rand_int());
        gint64 lo = (gint64)(((guint64)g_test_rand_int() << 32)
                             | g_test_rand_int());
        gint64 hi = (gint64)(((guint64)g_test_rand_int() << 32)
                             | g_test_rand_int());
        if (hi == lo)
            continue;
        GWY_ORDER(gint64, lo, hi);

        for (guint i = 0; i < niter; i++) {
            gint64 x = gwy_rand_int_range(rng, lo, hi);
            g_assert_cmpint(x, >=, lo);
            g_assert_cmpint(x, <, hi);
        }
    }

    gwy_rand_free(rng);
}

void
test_rand_range_double(void)
{
    enum { niter = 100000 };
    GwyRand *rng = gwy_rand_new_with_seed(g_test_rand_int());

    for (guint i = 0; i < niter; i++) {
        gdouble x = gwy_rand_double(rng);
        g_assert_cmpfloat(x, >, 0.0);
        g_assert_cmpfloat(x, <, 1.0);
    }

    gwy_rand_free(rng);
}

static void
count_bits_in_int(guint64 x, guint *counts, guint nc)
{
    for (guint i = 0; i < nc; i++) {
        if (x & 1)
            counts[i]++;
        x >>= 1;
    }
}

static void
count_excesses(guint *counts, guint *excesses, guint nc)
{
    for (guint i = 0; i < nc; i++) {
        if (abs(counts[i] - bit_samples_taken/2) > 25)
            excesses[i]++;
    }
}

static void
check_excesses(const guint *excesses, guint nc)
{
    for (guint i = 0; i < nc; i++) {
        g_assert_cmpfloat(fabs(excesses[i]/10000.0/(1.0 - P_500_25) - 1.0),
                          <=, 0.12);
    }
}

void
test_rand_uniformity_boolean(void)
{
    enum { niter = 10000, nbits = 1 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    guint counts[nbits], excesses[nbits];

    gwy_clear(excesses, nbits);
    for (guint i = 0; i < niter; i++) {
        gwy_rand_set_seed(rng, g_test_rand_int());
        gwy_clear(counts, nbits);
        for (guint j = 0; j < bit_samples_taken; j++) {
            guint64 x = gwy_rand_boolean(rng);
            count_bits_in_int(x, counts, nbits);
        }
        count_excesses(counts, excesses, nbits);
    }
    check_excesses(excesses, nbits);
    gwy_rand_free(rng);
}

void
test_rand_uniformity_byte(void)
{
    enum { niter = 10000, nbits = 8 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    guint counts[nbits], excesses[nbits];

    gwy_clear(excesses, nbits);
    for (guint i = 0; i < niter; i++) {
        gwy_rand_set_seed(rng, g_test_rand_int());
        gwy_clear(counts, nbits);
        for (guint j = 0; j < bit_samples_taken; j++) {
            guint64 x = gwy_rand_byte(rng);
            count_bits_in_int(x, counts, nbits);
        }
        count_excesses(counts, excesses, nbits);
    }
    check_excesses(excesses, nbits);
    gwy_rand_free(rng);
}

void
test_rand_uniformity_int64(void)
{
    enum { niter = 10000, nbits = 64 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    guint counts[nbits], excesses[nbits];

    gwy_clear(excesses, nbits);
    for (guint i = 0; i < niter; i++) {
        gwy_rand_set_seed(rng, g_test_rand_int());
        gwy_clear(counts, nbits);
        for (guint j = 0; j < bit_samples_taken; j++) {
            guint64 x = gwy_rand_int64(rng);
            count_bits_in_int(x, counts, nbits);
        }
        count_excesses(counts, excesses, nbits);
    }
    check_excesses(excesses, nbits);
    gwy_rand_free(rng);
}

static gboolean
kolmogorov_test(GwyRand *rng,
                guint64 seed,
                gdouble (*generate)(GwyRand*),
                gdouble (*cdf)(gdouble),
                gdouble *workspace,
                guint nsamples)
{
    gwy_rand_set_seed(rng, seed);

    for (guint i = 0; i < nsamples; i++)
        workspace[i] = generate(rng);

    gwy_math_sort(workspace, NULL, nsamples);

    gdouble D = 0;
    for (guint i = 0; i < nsamples; i++) {
        gdouble F = cdf(workspace[i]);
        gdouble ym = i/(gdouble)nsamples, yp = (i + 1)/(gdouble)nsamples;
        gdouble dm = fabs(F - ym), dp = fabs(F - yp);

        if (dm > D)
            D = dm;
        if (dp > D)
            D = dp;
    }

    // This is really, really improbable.
    g_assert_cmpfloat(D*sqrt(nsamples), <=, 4.0);
    // Failure that can occur occasionally; make noise only if we get several
    // of them.
    return D*sqrt(nsamples) < 2.0;
}

static gdouble
cdf_uniform(gdouble x)
{
    return x;
}

void
test_rand_distribution_uniform(void)
{
    enum { nsamples = 1000, niter = 100 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    gdouble *data = g_new(gdouble, nsamples);
    guint nfailures = 0;

    for (guint i = 0; i < niter; i++) {
        if (!kolmogorov_test(rng, g_test_rand_int(),
                             gwy_rand_double, cdf_uniform,
                             data, nsamples))
            nfailures++;
    }
    g_assert_cmpuint(nfailures, <=, 2);

    g_free(data);
    gwy_rand_free(rng);
}

static gdouble
cdf_exp_positive(gdouble x)
{
    return 1.0 - exp(-x);
}

void
test_rand_distribution_exp_positive(void)
{
    enum { nsamples = 1000, niter = 100 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    gdouble *data = g_new(gdouble, nsamples);
    guint nfailures = 0;

    for (guint i = 0; i < niter; i++) {
        if (!kolmogorov_test(rng, g_test_rand_int(),
                             gwy_rand_exp_positive, cdf_exp_positive,
                             data, nsamples))
            nfailures++;
    }
    g_assert_cmpuint(nfailures, <=, 2);

    g_free(data);
    gwy_rand_free(rng);
}

static gdouble
cdf_exp(gdouble x)
{
    return x <= 0.0 ? 0.5*exp(x) : 1.0 - 0.5*exp(-x);
}

void
test_rand_distribution_exp(void)
{
    enum { nsamples = 1000, niter = 100 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    gdouble *data = g_new(gdouble, nsamples);
    guint nfailures = 0;

    for (guint i = 0; i < niter; i++) {
        if (!kolmogorov_test(rng, g_test_rand_int(),
                             gwy_rand_exp, cdf_exp,
                             data, nsamples))
            nfailures++;
    }
    g_assert_cmpuint(nfailures, <=, 2);

    g_free(data);
    gwy_rand_free(rng);
}

static gdouble
cdf_normal_positive(gdouble x)
{
    return erf(x/G_SQRT2);
}

void
test_rand_distribution_normal_positive(void)
{
    enum { nsamples = 1000, niter = 100 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    gdouble *data = g_new(gdouble, nsamples);
    guint nfailures = 0;

    for (guint i = 0; i < niter; i++) {
        if (!kolmogorov_test(rng, g_test_rand_int(),
                             gwy_rand_normal_positive, cdf_normal_positive,
                             data, nsamples))
            nfailures++;
    }
    g_assert_cmpuint(nfailures, <=, 2);

    g_free(data);
    gwy_rand_free(rng);
}

static gdouble
cdf_normal(gdouble x)
{
    return 0.5*(1.0 + erf(x/G_SQRT2));
}

void
test_rand_distribution_normal(void)
{
    enum { nsamples = 1000, niter = 100 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    gdouble *data = g_new(gdouble, nsamples);
    guint nfailures = 0;

    for (guint i = 0; i < niter; i++) {
        if (!kolmogorov_test(rng, g_test_rand_int(),
                             gwy_rand_normal, cdf_normal,
                             data, nsamples))
            nfailures++;
    }
    g_assert_cmpuint(nfailures, <=, 2);

    g_free(data);
    gwy_rand_free(rng);
}

static gdouble
cdf_triangle_positive(gdouble x)
{
    return x*(2.0 - x);
}

void
test_rand_distribution_triangle_positive(void)
{
    enum { nsamples = 1000, niter = 100 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    gdouble *data = g_new(gdouble, nsamples);
    guint nfailures = 0;

    for (guint i = 0; i < niter; i++) {
        if (!kolmogorov_test(rng, g_test_rand_int(),
                             gwy_rand_triangle_positive, cdf_triangle_positive,
                             data, nsamples))
            nfailures++;
    }
    g_assert_cmpuint(nfailures, <=, 2);

    g_free(data);
    gwy_rand_free(rng);
}

static gdouble
cdf_triangle(gdouble x)
{
    return 0.5*x*(2.0 - fabs(x)) + 0.5;
}

void
test_rand_distribution_triangle(void)
{
    enum { nsamples = 1000, niter = 100 };

    GwyRand *rng = gwy_rand_new_with_seed(0);
    gdouble *data = g_new(gdouble, nsamples);
    guint nfailures = 0;

    for (guint i = 0; i < niter; i++) {
        if (!kolmogorov_test(rng, g_test_rand_int(),
                             gwy_rand_triangle, cdf_triangle,
                             data, nsamples))
            nfailures++;
    }
    g_assert_cmpuint(nfailures, <=, 2);

    g_free(data);
    gwy_rand_free(rng);
}

/* vim: set cin et ts=4 sw=4 cino=>1s,e0,n0,f0,{0,}0,^0,\:1s,=0,g1s,h0,t0,+1s,c3,(0,u0 : */

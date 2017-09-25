///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2012-2017 DreamWorks Animation LLC
//
// All rights reserved. This software is distributed under the
// Mozilla Public License 2.0 ( http://www.mozilla.org/MPL/2.0/ )
//
// Redistributions of source code must retain the above copyright
// and license notice and the following restrictions and disclaimer.
//
// *     Neither the name of DreamWorks Animation nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// IN NO EVENT SHALL THE COPYRIGHT HOLDERS' AND CONTRIBUTORS' AGGREGATE
// LIABILITY FOR ALL CLAIMS REGARDLESS OF THEIR BASIS EXCEED US$250.00.
//
///////////////////////////////////////////////////////////////////////////

//#define FAST_SWEEPING_DEBUG

#include <sstream>
#include <cppunit/extensions/HelperMacros.h>
#include <openvdb/Types.h>
#include <openvdb/openvdb.h>
#include <openvdb/tools/ChangeBackground.h>
#include <openvdb/tools/Diagnostics.h>
#include <openvdb/tools/FastSweeping.h>
#include <openvdb/tools/LevelSetSphere.h>
#include <openvdb/tools/LevelSetTracker.h>
#include <openvdb/tools/LevelSetRebuild.h>
#include <openvdb/tools/LevelSetPlatonic.h>
#include <openvdb/tools/LevelSetUtil.h>

#ifdef FAST_SWEEPING_DEBUG
#include <openvdb/util/CpuTimer.h>
// Uncomment to test on models from our web-site
//#define TestFastSweeping_DATA_PATH "/home/kmu/src/data/vdb/"
//#define TestFastSweeping_DATA_PATH "/usr/pic1/Data/OpenVDB/LevelSetModels/"
#endif


class TestFastSweeping: public CppUnit::TestFixture
{
public:
    virtual void setUp() { openvdb::initialize(); }
    virtual void tearDown() { openvdb::uninitialize(); }

    CPPUNIT_TEST_SUITE(TestFastSweeping);
    CPPUNIT_TEST(testDilateSDF);
    CPPUNIT_TEST(testExtendSDF);
    CPPUNIT_TEST(testMakeSDF);

    CPPUNIT_TEST_SUITE_END();

    void testDilateSDF();
    void testExtendSDF();
    void testMakeSDF();
    
    void write(const std::string &name, openvdb::FloatGrid::Ptr grid);
    
};// TestFastSweeping

CPPUNIT_TEST_SUITE_REGISTRATION(TestFastSweeping);

void
TestFastSweeping::write(const std::string &name, openvdb::FloatGrid::Ptr grid)
{
    openvdb::GridPtrVec grids;
    grids.push_back(grid);
    openvdb::io::File file(name);
    file.write(grids);
}

void
TestFastSweeping::testDilateSDF()
{
    using namespace openvdb;
    // Define parameters for the level set sphere to be re-normalized
    const float radius = 200.0f;
    const Vec3f center(0.0f, 0.0f, 0.0f);
    const float voxelSize = 1.0f;//half width
    const int width = 3, new_width = 50;//half width

    FloatGrid::Ptr grid = tools::createLevelSetSphere<FloatGrid>(radius, center, voxelSize, float(width));
    const size_t oldVoxelCount = grid->activeVoxelCount();

    tools::FastSweeping<FloatGrid> fs(*grid);
    CPPUNIT_ASSERT_EQUAL(size_t(0), fs.sweepingVoxelCount());
    CPPUNIT_ASSERT_EQUAL(size_t(0), fs.boundaryVoxelCount());
    fs.initDilateSDF(new_width - width);
    CPPUNIT_ASSERT(fs.sweepingVoxelCount() > 0);
    CPPUNIT_ASSERT(fs.boundaryVoxelCount() > 0);
    fs.sweep();
    CPPUNIT_ASSERT(fs.sweepingVoxelCount() > 0);
    CPPUNIT_ASSERT(fs.boundaryVoxelCount() > 0);
    fs.clear();
    CPPUNIT_ASSERT_EQUAL(size_t(0), fs.sweepingVoxelCount());
    CPPUNIT_ASSERT_EQUAL(size_t(0), fs.boundaryVoxelCount());
    const size_t sweepingVoxelCount = grid->activeVoxelCount();
    CPPUNIT_ASSERT(sweepingVoxelCount > oldVoxelCount);

    {// Check that the norm of the gradient for all active voxels is close to unity
        tools::Diagnose<FloatGrid> diagnose(*grid);
        tools::CheckNormGrad<FloatGrid> test(*grid, 0.99f, 1.01f);
        const std::string message = diagnose.check(test,
                                                   false,// don't generate a mask grid
                                                   true,// check active voxels
                                                   false,// ignore active tiles since a level set has none
                                                   false);// no need to check the background value
        CPPUNIT_ASSERT(message.empty());
        CPPUNIT_ASSERT_EQUAL(size_t(0), diagnose.failureCount());
        //std::cout << "\nOutput 1: " << message << std::endl;
    }
    {// Make sure all active voxels fail the following test
        tools::Diagnose<FloatGrid> diagnose(*grid);
        tools::CheckNormGrad<FloatGrid> test(*grid, std::numeric_limits<float>::min(), 0.99f);
        const std::string message = diagnose.check(test,
                                                   false,// don't generate a mask grid
                                                   true,// check active voxels
                                                   false,// ignore active tiles since a level set has none
                                                   false);// no need to check the background value
        CPPUNIT_ASSERT(!message.empty());
        CPPUNIT_ASSERT_EQUAL(sweepingVoxelCount, diagnose.failureCount());
        //std::cout << "\nOutput 2: " << message << std::endl;
    }
    {// Make sure all active voxels fail the following test
        tools::Diagnose<FloatGrid> diagnose(*grid);
        tools::CheckNormGrad<FloatGrid> test(*grid, 1.01f, std::numeric_limits<float>::max());
        const std::string message = diagnose.check(test,
                                                   false,// don't generate a mask grid
                                                   true,// check active voxels
                                                   false,// ignore active tiles since a level set has none
                                                   false);// no need to check the background value
        CPPUNIT_ASSERT(!message.empty());
        CPPUNIT_ASSERT_EQUAL(sweepingVoxelCount, diagnose.failureCount());
        //std::cout << "\nOutput 3: " << message << std::endl;
    }
}// testDilateSDF


void
TestFastSweeping::testExtendSDF()
{
    using namespace openvdb;
    // Define parameterS FOR the level set sphere to be re-normalized
    const float radius = 200.0f;
    const Vec3f center(0.0f, 0.0f, 0.0f);
    const float voxelSize = 1.0f, width = 3.0f;//half width
    const float new_width = 50;
    
    {// Use box as a mask
        //std::cerr << "\nUse box as a mask" << std::endl;
        FloatGrid::Ptr grid = tools::createLevelSetSphere<FloatGrid>(radius, center, voxelSize, width);
        CoordBBox bbox(Coord(150,-50,-50), Coord(250,50,50));
        MaskGrid mask;
        mask.sparseFill(bbox, true);
#ifdef FAST_SWEEPING_DEBUG
        this->write("/tmp/box_mask_input.vdb", grid);
        util::CpuTimer timer("\nParallel sparse fast sweeping with a box mask");
#endif

        tools::extendSDF(*grid, mask);
        //tools::FastSweeping<FloatGrid> fs(*grid);
        //fs.initExtendSDF(mask);
        //fs.sweep();
        //std::cerr << "voxel count = " << fs.sweepingVoxelCount() << std::endl;
        //std::cerr << "boundary count = " << fs.boundaryVoxelCount() << std::endl;
        //CPPUNIT_ASSERT(fs.sweepingVoxelCount() > 0);
#ifdef FAST_SWEEPING_DEBUG
        timer.stop();
        this->write("/tmp/box_mask_output.vdb", grid);
#endif
       
        {// Check that the norm of the gradient for all active voxels is close to unity
            tools::Diagnose<FloatGrid> diagnose(*grid);
            tools::CheckNormGrad<FloatGrid> test(*grid, 0.99f, 1.01f);
            const std::string message = diagnose.check(test,
                                                       false,// don't generate a mask grid
                                                       true,// check active voxels
                                                       false,// ignore active tiles since a level set has none
                                                       false);// no need to check the background value
            //std::cerr << message << std::endl;
            const double percent = 100.0*double(diagnose.failureCount())/double(grid->activeVoxelCount());
            //std::cerr << "Failures = " << percent << "%" << std::endl;
            //std::cerr << "Failed: " << diagnose.failureCount() << std::endl;
            //std::cerr << "Total : " << grid->activeVoxelCount() << std::endl;
            CPPUNIT_ASSERT(percent < 0.01);
            //CPPUNIT_ASSERT(message.empty());
            //CPPUNIT_ASSERT_EQUAL(size_t(0), diagnose.failureCount());
        }
    }

    {// Use sphere as a mask
        //std::cerr << "\nUse sphere as a mask" << std::endl;
        FloatGrid::Ptr grid = tools::createLevelSetSphere<FloatGrid>(radius, center, voxelSize, width);
        FloatGrid::Ptr mask = tools::createLevelSetSphere<FloatGrid>(radius, center, voxelSize, new_width);
#ifdef  FAST_SWEEPING_DEBUG
        this->write("/tmp/sphere_mask_input.vdb", grid);
        util::CpuTimer timer("\nParallel sparse fast sweeping with a sphere mask");
#endif
        tools::extendSDF(*grid, *mask);
        //tools::FastSweeping<FloatGrid> fs(*grid);
        //fs.initExtendSDF(*mask);
        //fs.sweep();
#ifdef FAST_SWEEPING_DEBUG
        timer.stop();
        this->write("/tmp/sphere_mask_output.vdb", grid);
#endif
        //std::cerr << "voxel count = " << fs.sweepingVoxelCount() << std::endl;
        //std::cerr << "boundary count = " << fs.boundaryVoxelCount() << std::endl;
        //CPPUNIT_ASSERT(fs.sweepingVoxelCount() > 0);
        
        {// Check that the norm of the gradient for all active voxels is close to unity
            tools::Diagnose<FloatGrid> diagnose(*grid);
            tools::CheckNormGrad<FloatGrid> test(*grid, 0.99f, 1.01f);
            const std::string message = diagnose.check(test,
                                                       false,// don't generate a mask grid
                                                       true,// check active voxels
                                                       false,// ignore active tiles since a level set has none
                                                       false);// no need to check the background value
            //std::cerr << message << std::endl;
            const double percent = 100.0*double(diagnose.failureCount())/double(grid->activeVoxelCount());
            //std::cerr << "Failures = " << percent << "%" << std::endl;
            //std::cerr << "Failed: " << diagnose.failureCount() << std::endl;
            //std::cerr << "Total : " << grid->activeVoxelCount() << std::endl;
            //CPPUNIT_ASSERT(message.empty());
            //CPPUNIT_ASSERT_EQUAL(size_t(0), diagnose.failureCount());
            CPPUNIT_ASSERT(percent < 0.01);
            //std::cout << "\nOutput 1: " << message << std::endl;
        }
    }
    
    {// Use dodecahedron as a mask
        //std::cerr << "\nUse dodecahedron as a mask" << std::endl;
        FloatGrid::Ptr grid = tools::createLevelSetSphere<FloatGrid>(radius, center, voxelSize, width);
        FloatGrid::Ptr mask = tools::createLevelSetDodecahedron<FloatGrid>(50, Vec3f(radius, 0.0f, 0.0f),
                                                                           voxelSize, 10);
#ifdef FAST_SWEEPING_DEBUG
        this->write("/tmp/dodecahedron_mask_input.vdb", grid);
        util::CpuTimer timer("\nParallel sparse fast sweeping with a dodecahedron mask");
#endif
        
        tools::extendSDF(*grid, *mask);
        //tools::FastSweeping<FloatGrid> fs(*grid);
        //fs.initExtendSDF(*mask);
        //std::cerr << "voxel count = " << fs.sweepingVoxelCount() << std::endl;
        //std::cerr << "boundary count = " << fs.boundaryVoxelCount() << std::endl;
        //CPPUNIT_ASSERT(fs.sweepingVoxelCount() > 0);
        //fs.sweep();
        //fs.clear();
#ifdef FAST_SWEEPING_DEBUG
        timer.stop();
        this->write("/tmp/dodecahedron_mask_output.vdb", grid);
#endif
       
        {// Check that the norm of the gradient for all active voxels is close to unity
            tools::Diagnose<FloatGrid> diagnose(*grid);
            tools::CheckNormGrad<FloatGrid> test(*grid, 0.99f, 1.01f);
            const std::string message = diagnose.check(test,
                                                       false,// don't generate a mask grid
                                                       true,// check active voxels
                                                       false,// ignore active tiles since a level set has none
                                                       false);// no need to check the background value
            //std::cerr << message << std::endl;
            const double percent = 100.0*double(diagnose.failureCount())/double(grid->activeVoxelCount());
            //std::cerr << "Failures = " << percent << "%" << std::endl;
            //std::cerr << "Failed: " << diagnose.failureCount() << std::endl;
            //std::cerr << "Total : " << grid->activeVoxelCount() << std::endl;
            //CPPUNIT_ASSERT(message.empty());
            //CPPUNIT_ASSERT_EQUAL(size_t(0), diagnose.failureCount());
            CPPUNIT_ASSERT(percent < 0.01);
            //std::cout << "\nOutput 1: " << message << std::endl;
        }
    }
#ifdef TestFastSweeping_DATA_PATH
     {// Use bunny as a mask
         //std::cerr << "\nUse bunny as a mask" << std::endl;
         FloatGrid::Ptr grid = tools::createLevelSetSphere<FloatGrid>(10.0f, Vec3f(-10,0,0), 0.05f, width);
         openvdb::initialize();//required whenever I/O of OpenVDB files is performed!
         const std::string path(TestFastSweeping_DATA_PATH);
         io::File file( path + "bunny.vdb" );
         file.open(false);//disable delayed loading
         FloatGrid::Ptr mask = openvdb::gridPtrCast<openvdb::FloatGrid>(file.getGrids()->at(0));
         
         this->write("/tmp/bunny_mask_input.vdb", grid);
         tools::FastSweeping<FloatGrid> fs(*grid);
         util::CpuTimer timer("\nParallel sparse fast sweeping with a bunny mask");

         fs.initExtendSDF(*mask);
         //std::cerr << "voxel count = " << fs.sweepingVoxelCount() << std::endl;
         //std::cerr << "boundary count = " << fs.boundaryVoxelCount() << std::endl;
         fs.sweep();
         fs.clear();
         timer.stop();

         this->write("/tmp/bunny_mask_output.vdb", grid);
         {// Check that the norm of the gradient for all active voxels is close to unity
             tools::Diagnose<FloatGrid> diagnose(*grid);
             tools::CheckNormGrad<FloatGrid> test(*grid, 0.99f, 1.01f);
             const std::string message = diagnose.check(test,
                                                        false,// don't generate a mask grid
                                                        true,// check active voxels
                                                        false,// ignore active tiles since a level set has none
                                                        false);// no need to check the background value
             //std::cerr << message << std::endl;
             const double percent = 100.0*double(diagnose.failureCount())/double(grid->activeVoxelCount());
             //std::cerr << "Failures = " << percent << "%" << std::endl;
             //std::cerr << "Failed: " << diagnose.failureCount() << std::endl;
             //std::cerr << "Total : " << grid->activeVoxelCount() << std::endl;
             //CPPUNIT_ASSERT(message.empty());
             //CPPUNIT_ASSERT_EQUAL(size_t(0), diagnose.failureCount());
             CPPUNIT_ASSERT(percent < 4.5);// crossing characteristics!
             //std::cout << "\nOutput 1: " << message << std::endl;
         }
     }
#endif
}// testExtendSDF

void
TestFastSweeping::testMakeSDF()
{
    using namespace openvdb;
    // Define parameterS FOR the level set sphere to be re-normalized
    const float radius = 200.0f;
    const Vec3f center(0.0f, 0.0f, 0.0f);
    const float voxelSize = 1.0f, width = 3.0f;//half width

    FloatGrid::Ptr grid = tools::createLevelSetSphere<FloatGrid>(radius, center, voxelSize, float(width));
    tools::sdfToFogVolume(*grid);
    const size_t sweepingVoxelCount = grid->activeVoxelCount();
#ifdef FAST_SWEEPING_DEBUG
    this->write("/tmp/ls_input.vdb", grid);
#endif
    tools::FastSweeping<FloatGrid> fs(*grid);
#ifdef FAST_SWEEPING_DEBUG
    util::CpuTimer timer("\nParallel sparse fast sweeping with a fog volume");
#endif
    fs.initMakeSDF(0.5);
    CPPUNIT_ASSERT(fs.sweepingVoxelCount() > 0);
    //std::cerr << "voxel count = " << fs.sweepingVoxelCount() << std::endl;
    //std::cerr << "boundary count = " << fs.boundaryVoxelCount() << std::endl;
    fs.sweep();
#ifdef FAST_SWEEPING_DEBUG
    timer.restart("getSweepingGrid");
#endif
    auto tmp = fs.getSweepingGrid();
#ifdef FAST_SWEEPING_DEBUG
    timer.stop();
#endif
    fs.clear();
    CPPUNIT_ASSERT(sweepingVoxelCount > tmp->activeVoxelCount());
#ifdef FAST_SWEEPING_DEBUG
    this->write("/tmp/sweepingGrid.vdb", tmp);
#endif
    CPPUNIT_ASSERT_EQUAL(sweepingVoxelCount, grid->activeVoxelCount());
#ifdef FAST_SWEEPING_DEBUG
    this->write("/tmp/fog_output.vdb", grid);
#endif
    {// Check that the norm of the gradient for all active voxels is close to unity
        tools::Diagnose<FloatGrid> diagnose(*grid);
        tools::CheckNormGrad<FloatGrid> test(*grid, 0.99f, 1.01f);
        const std::string message = diagnose.check(test,
                                                   false,// don't generate a mask grid
                                                   true,// check active voxels
                                                   false,// ignore active tiles since a level set has none
                                                   false);// no need to check the background value
        //std::cerr << message << std::endl;
        const double percent = 100.0*double(diagnose.failureCount())/double(grid->activeVoxelCount());
        //std::cerr << "Failures = " << percent << "%" << std::endl;
        //std::cerr << "Failed: " << diagnose.failureCount() << std::endl;
        //std::cerr << "Total : " << grid->activeVoxelCount() << std::endl;
        CPPUNIT_ASSERT(percent < 3.0);
    }
}// testMakeSDF

// Copyright (c) 2012-2017 DreamWorks Animation LLC
// All rights reserved. This software is distributed under the
// Mozilla Public License 2.0 ( http://www.mozilla.org/MPL/2.0/ )

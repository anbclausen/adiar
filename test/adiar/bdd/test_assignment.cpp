go_bandit([]() {
  describe("adiar/bdd/assignment.cpp", []() {
    ptr_t sink_T = create_sink_ptr(true);
    ptr_t sink_F = create_sink_ptr(false);

    node_file bdd_1;
    /*
              1      ---- x0
             / \
             2 |     ---- x1
            / \|
           3   4     ---- x2
          / \ / \
          F 5 T F    ---- x3
           / \
           F T
    */

    {
      node_t n5 = create_node(3,0, sink_F, sink_T);
      node_t n4 = create_node(2,1, sink_T, sink_F);
      node_t n3 = create_node(2,0, sink_F, n5.uid);
      node_t n2 = create_node(1,0, n3.uid, n4.uid);
      node_t n1 = create_node(0,0, n2.uid, n4.uid);

      node_writer nw(bdd_1);
      nw << n5 << n4 << n3 << n2 << n1;
    }

    node_file bdd_2;
    /*
               1       ---- x0
              / \
             2   \     ---- x1
            / \   \
           3   4   5   ---- x2
          / \ / \ / \
          F | T F | T  ---- x3
            \__ __/
               6
              / \
              T F
    */

    { // Garbage collect writer to free write-lock
      node_t n6 = create_node(3,0, sink_T, sink_F);
      node_t n5 = create_node(2,2, n6.uid, sink_T);
      node_t n4 = create_node(2,1, sink_T, sink_F);
      node_t n3 = create_node(2,0, sink_F, n6.uid);
      node_t n2 = create_node(1,0, n3.uid, n4.uid);
      node_t n1 = create_node(0,0, n2.uid, n5.uid);

      node_writer nw(bdd_2);
      nw << n6 << n5 << n4 << n3 << n2 << n1;
    }

    node_file bdd_3;
    /*

                      1     ---- x1
                     / \
                     2  \   ---- x3
                    / \ /
                   3   4    ---- x5
                  / \ / \
                  T F F T
    */

    { // Garbage collect writer to free write-lock
      node_t n4 = create_node(5,1, sink_F, sink_T);
      node_t n3 = create_node(5,0, sink_T, sink_F);
      node_t n2 = create_node(3,0, n3.uid, n4.uid);
      node_t n1 = create_node(1,0, n2.uid, n4.uid);

      node_writer nw(bdd_3);
      nw << n4 << n3 << n2 << n1;
    }

    describe("bdd_satmin(f)", [&]() {
      it("should retrieve assignment from true sink", [&]() {
        node_file T;
        {
          node_writer nw(T);
          nw << create_sink(true);
        }

        assignment_file result = bdd_satmin(T);

        assignment_stream<> out_assignment(result);
        AssertThat(out_assignment.can_pull(), Is().False());
      });

      it("should retrieve assignment from false sink", [&]() {
        node_file F;
        {
          node_writer nw(F);
          nw << create_sink(false);
        }

        assignment_file result = bdd_satmin(false);

        assignment_stream<> out_assignment(result);
        AssertThat(out_assignment.can_pull(), Is().False());
      });

      it("should retrieve assignment [1]", [&]() {
        assignment_file result = bdd_satmin(bdd_1);
        assignment_stream<> out_assignment(result);

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(0, false)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(1, false)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(2, true)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(3, true)));

        AssertThat(out_assignment.can_pull(), Is().False());
      });

      it("should retrieve assignment [1]", [&]() {
        assignment_file result = bdd_satmin(bdd_1);
        assignment_stream<> out_assignment(result);

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(0, false)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(1, false)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(2, true)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(3, true)));

        AssertThat(out_assignment.can_pull(), Is().False());
      });

      it("should retrieve assignment [~1]", [&]() {
        assignment_file result = bdd_satmin(bdd_not(bdd_1));
        assignment_stream<> out_assignment(result);

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(0, false)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(1, false)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(2, false)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(3, false)));

        AssertThat(out_assignment.can_pull(), Is().False());
      });

      it("should retrieve assignment [2]", [&]() {
        assignment_file result = bdd_satmin(bdd_2);
        assignment_stream<> out_assignment(result);

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(0, false)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(1, false)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(2, true)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(3, false)));

        AssertThat(out_assignment.can_pull(), Is().False());
      });

      it("should retrieve assignment [3]", [&]() {
        assignment_file result = bdd_satmin(bdd_3);
        assignment_stream<> out_assignment(result);

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(1, false)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(3, false)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(5, false)));

        AssertThat(out_assignment.can_pull(), Is().False());
      });

      it("should retrieve assignment [3]", [&]() {
        assignment_file result = bdd_satmin(bdd_not(bdd_3));
        assignment_stream<> out_assignment(result);

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(1, false)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(3, false)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(5, true)));

        AssertThat(out_assignment.can_pull(), Is().False());
      });
    });

    describe("bdd_satmax", [&]() {
      it("should retrieve maximal assignment [1]", [&]() {
        assignment_file result = bdd_satmax(bdd_1);
        assignment_stream<> out_assignment(result);

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(0, true)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(1, true)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(2, false)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(3, true)));

        AssertThat(out_assignment.can_pull(), Is().False());
      });

      it("should retrieve maximal assignment [~1]", [&]() {
        assignment_file result = bdd_satmax(bdd_not(bdd_1));
        assignment_stream<> out_assignment(result);

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(0, true)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(1, true)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(2, true)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(3, true)));

        AssertThat(out_assignment.can_pull(), Is().False());
      });

      it("should retrieve maximal assignment [2]", [&]() {
        assignment_file result = bdd_satmax(bdd_2);
        assignment_stream<> out_assignment(result);

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(0, true)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(1, true)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(2, true)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(3, true)));

        AssertThat(out_assignment.can_pull(), Is().False());
      });

      it("should retrieve maximal assignment [~2]", [&]() {
        assignment_file result = bdd_satmax(bdd_not(bdd_2));
        assignment_stream<> out_assignment(result);

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(0, true)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(1, true)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(2, false)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(3, true)));

        AssertThat(out_assignment.can_pull(), Is().False());
      });

      it("should retrieve maximal assignment [3]", [&]() {
        assignment_file result = bdd_satmax(bdd_3);
        assignment_stream<> out_assignment(result);

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(1, true)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(3, true)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(5, true)));

        AssertThat(out_assignment.can_pull(), Is().False());
      });

      it("should retrieve maximal assignment [3]", [&]() {
        assignment_file result = bdd_satmax(bdd_not(bdd_3));
        assignment_stream<> out_assignment(result);

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(1, true)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(3, true)));

        AssertThat(out_assignment.can_pull(), Is().True());
        AssertThat(out_assignment.pull(), Is().EqualTo(create_assignment(5, false)));

        AssertThat(out_assignment.can_pull(), Is().False());
      });
    });
  });
 });

#ifndef ODR_ANY_THING_H
#define ODR_ANY_THING_H

namespace odr {

template <typename Thing> class AnyThing {
public:
  using Base = AnyThing<Thing>;

  AnyThing() = default;

  AnyThing(const AnyThing &other) { copy_(other); }

  AnyThing(AnyThing &&other) noexcept { move_(std::move(other)); }

  template <typename Derived, typename = typename std::enable_if<
                                  std::is_base_of<Thing, Derived>::value>::type>
  explicit AnyThing(Derived &derived)
      : m_thing{&derived}, m_info{Info<Derived>::instance()}, m_ownership{
                                                                  false} {}

  template <typename Derived, typename = typename std::enable_if<
                                  std::is_base_of<Thing, Derived>::value>::type>
  explicit AnyThing(Derived &&derived)
      : m_thing{new Derived(derived)}, m_info{Info<Derived>::instance()},
        m_ownership{true} {}

  ~AnyThing() {
    if (m_ownership) {
      m_info->destruct(m_thing);
    }
  }

  AnyThing &operator=(const AnyThing &other) {
    if (&other != this) {
      copy_(other);
    }
    return *this;
  }

  AnyThing &operator=(AnyThing &&other) noexcept {
    move_(std::move(other));
    return *this;
  }

  bool operator==(const AnyThing &rhs) const {
    if (m_thing == rhs.m_thing) {
      return true;
    }
    if ((m_thing == nullptr) || (m_thing == nullptr)) {
      return false;
    }
    return m_info->equals(*m_thing, *rhs.m_thing);
  }

  bool operator!=(const AnyThing &rhs) const {
    if (m_thing == rhs.m_thing) {
      return false;
    }
    if ((m_thing == nullptr) || (m_thing == nullptr)) {
      return true;
    }
    return m_info->not_equals(*m_thing, *rhs.m_thing);
  }

  explicit operator bool() const { return m_thing != nullptr; }

protected:
  Thing *operator->() { return m_thing; }

  const Thing *operator->() const { return m_thing; }

  Thing *get() { return m_thing; }

  const Thing *get() const { return m_thing; }

private:
  class InfoBase {
  public:
    [[nodiscard]] virtual std::size_t size() const = 0;
    virtual Thing *copy_construct(const Thing &from) const = 0;
    virtual Thing *copy_construct_to(const Thing &from, void *to) const = 0;
    virtual Thing *move_construct(Thing &&from) const = 0;
    virtual Thing *move_construct_to(Thing &&from, void *to) const = 0;
    virtual void destruct(const Thing *a) const noexcept = 0;
    virtual bool equals(const Thing &a, const Thing &b) const = 0;
    virtual bool not_equals(const Thing &a, const Thing &b) const = 0;
  };

  template <typename Derived> class Info final : public InfoBase {
  public:
    static InfoBase *instance() {
      static Info<Derived> instance;
      return &instance;
    }
    [[nodiscard]] std::size_t size() const final { return sizeof(Derived); }
    Thing *copy_construct(const Thing &from) const final {
      return new Derived(static_cast<const Derived &>(from));
    }
    Thing *copy_construct_to(const Thing &from, void *to) const final {
      return new (to) Derived(static_cast<const Derived &>(from));
    }
    Thing *move_construct(Thing &&from) const final {
      return new Derived(static_cast<Derived &&>(from));
    }
    Thing *move_construct_to(Thing &&from, void *to) const final {
      return new (to) Derived(static_cast<Derived &&>(from));
    }
    void destruct(const Thing *a) const noexcept final { delete a; }
    bool equals(const Thing &a, const Thing &b) const final {
      return static_cast<const Derived &>(a) == static_cast<const Derived &>(b);
    }
    bool not_equals(const Thing &a, const Thing &b) const final {
      return static_cast<const Derived &>(a) != static_cast<const Derived &>(b);
    }
  };

  void copy_(const AnyThing &other) {
    if (other.m_thing == nullptr) {
      return;
    }

    m_info = other.m_info;
    if (other.m_ownership) {
      m_thing = other.m_info->copy_construct(*other.m_thing);
      m_ownership = true;
    } else {
      m_thing = other.m_thing;
    }
  }

  void move_(AnyThing &&other) {
    std::swap(m_thing, other.m_thing);
    std::swap(m_info, other.m_info);
    std::swap(m_ownership, other.m_ownership);
  }

  Thing *m_thing{nullptr};
  InfoBase *m_info{nullptr};
  bool m_ownership{false};
};

} // namespace odr

#endif // ODR_ANY_THING_H
